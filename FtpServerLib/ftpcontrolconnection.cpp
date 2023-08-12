#include "ftpcontrolconnection.h"
#include "ftplistcommand.h"
#include "ftpretrcommand.h"
#include "ftpstorcommand.h"
#include "sslserver.h"
#include "dataconnection.h"

#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QStringList>
#include <QDir>
#include <QEventLoop>
#include <QDebug>
#include <QTimer>
#include <QSslSocket>
#include <QTextCodec>

FtpControlConnection::FtpControlConnection(QObject *parent, QSslSocket *socket, const QString &rootPath, const QString &userName, const QString &password, bool readOnly) :
    QObject(parent)
{
    this->m_socket = socket;
    this->m_userName = userName;
    this->m_password = password;
    this->m_rootPath = rootPath;
    this->m_readOnly = readOnly;
    m_isLoggedIn = false;
    m_encryptDataConnection = false;
    socket->setParent(this);
    connect(socket, SIGNAL(readyRead()), this, SLOT(acceptNewData()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(deleteLater())); // socket disconnected后析构FtpControlConnection
    connect(socket, &QSslSocket::disconnected, [this, socket]() {
        emit connectionClosed(socket->peerAddress().toString());
    });
    m_currentDirectory = "/";
    m_dataConnection = new DataConnection(this);
    reply("220 Welcome to FtpServer.");
    qDebug() << "FtpControlConnection()";
}

FtpControlConnection::~FtpControlConnection()
{
    qDebug() << "~FtpControlConnection()";
}

bool isGBK(unsigned char* data, int len) 
{
	int i = 0;
	while (i < len)
    {
		if (data[i] <= 0x7f) 
        {
			//编码小于等于127,只有一个字节的编码，兼容ASCII
			i++;
			continue;
		}
		else 
        {
			//大于127的使用双字节编码
			if (data[i] >= 0x81 &&
				data[i] <= 0xfe &&
				data[i + 1] >= 0x40 &&
				data[i + 1] <= 0xfe &&
				data[i + 1] != 0xf7)
            {
				i += 2;
				continue;
			}
			else
            {
				return false;
			}
		}
	}
	return true;
}

bool is_str_utf8(const char* str)
{
	unsigned int nBytes = 0;//UFT8可用1-6个字节编码,ASCII用一个字节  
	unsigned char chr = *str;
	bool bAllAscii = true;

	for (unsigned int i = 0; str[i] != '\0'; ++i) 
    {
		chr = *(str + i);
		//判断是否ASCII编码,如果不是,说明有可能是UTF8,ASCII用7位编码,最高位标记为0,0xxxxxxx 
		if (nBytes == 0 && (chr & 0x80) != 0)
        {
			bAllAscii = false;
		}

		if (nBytes == 0) 
        {
			//如果不是ASCII码,应该是多字节符,计算字节数  
			if (chr >= 0x80) 
            {

				if (chr >= 0xFC && chr <= 0xFD) 
                {
					nBytes = 6;
				}
				else if (chr >= 0xF8) 
                {
					nBytes = 5;
				}
				else if (chr >= 0xF0) 
                {
					nBytes = 4;
				}
				else if (chr >= 0xE0)
                {
					nBytes = 3;
				}
				else if (chr >= 0xC0)
                {
					nBytes = 2;
				}
				else 
                {
					return false;
				}

				nBytes--;
			}
		}
		else 
        {
			//多字节符的非首字节,应为 10xxxxxx 
			if ((chr & 0xC0) != 0x80) 
            {
				return false;
			}
			//减到为零为止
			nBytes--;
		}
	}

	//违返UTF8编码规则 
	if (nBytes != 0) 
    {
		return false;
	}

	if (bAllAscii) 
    { //如果全部都是ASCII, 也是UTF8
		return true;
	}

	return true;
}

bool is_str_gbk(const char* str)
{
	unsigned int nBytes = 0;//GBK可用1-2个字节编码,中文两个 ,英文一个 
	unsigned char chr = *str;
	bool bAllAscii = true; //如果全部都是ASCII,  

	for (unsigned int i = 0; str[i] != '\0'; ++i) 
    {
		chr = *(str + i);
		if ((chr & 0x80) != 0 && nBytes == 0)
        {// 判断是否ASCII编码,如果不是,说明有可能是GBK
			bAllAscii = false;
		}

		if (nBytes == 0)
        {
			if (chr >= 0x80) 
            {
				if (chr >= 0x81 && chr <= 0xFE) 
                {
					nBytes = +2;
				}
				else 
                {
					return false;
				}

				nBytes--;
			}
		}
		else 
        {
			if (chr < 0x40 || chr>0xFE) 
            {
				return false;
			}
			nBytes--;
		}//else end
	}

	if (nBytes != 0) 
    {		//违返规则 
		return false;
	}

	if (bAllAscii) 
    { //如果全部都是ASCII, 也是GBK
		return true;
	}

	return true;
}

void FtpControlConnection::acceptNewData()
{
    if (!m_socket->canReadLine()) 
    {
        return;
    }

    // Note how we execute only one line, and use QTimer::singleShot, instead
    // of using a for-loop until no more lines are available. This is done
    // so we don't block the event loop for a long time.

	QByteArray ba = m_socket->readLine().trimmed();
	//qDebug() << "socket read: " << ba;
	//qDebug() << "from local8bit: " << QString::fromLocal8Bit(ba);

	QTextCodec::ConverterState state;
	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
	QString text = codec->toUnicode(ba.constData(), ba.size(), &state);
	if (state.invalidChars > 0) 
	{
		qDebug() << "It is gbk";
		processCommand(QString::fromLocal8Bit(ba));
	}
	else
	{
		//qDebug() << "It is Utf8";
		processCommand(text);
	}

    QTimer::singleShot(0, this, SLOT(acceptNewData()));
}

void FtpControlConnection::disconnectFromHost()
{
    m_socket->disconnectFromHost();
}

bool FtpControlConnection::verifyAuthentication(const QString &command)
{
    if (m_isLoggedIn) 
    {
        return true;
    }

	// cmd ftp在登录之前就会发送OPTS UTF8 ON命令，所以注释掉，避免cmd使用gbk编码导致中文乱码
    const char *commandsRequiringAuth[] = 
    {
        "PWD", "CWD", "TYPE", "PORT", "PASV", "LIST", "RETR", "REST",
        "NLST", "SIZE", "SYST", "PROT", "CDUP", /*"OPTS",*/ "PBSZ", "NOOP",
        "STOR", "MKD", "RMD", "DELE", "RNFR", "RNTO", "APPE",
        "XPWD"
    };

    for (size_t ii = 0; ii < sizeof(commandsRequiringAuth)/sizeof(commandsRequiringAuth[0]); ++ii) 
    {
        if (command == commandsRequiringAuth[ii]) 
        {
            reply("530 You must log in first.");
            return false;
        }
    }

    return true;
}

bool FtpControlConnection::verifyWritePermission(const QString &command)
{
    if (!m_readOnly) 
    {
        return true;
    }

    const char *commandsRequiringWritePermission[] = 
    {
        "STOR", "MKD", "RMD", "DELE", "RNFR", "RNTO", "APPE"
    };

    for (size_t ii = 0; ii < sizeof(commandsRequiringWritePermission)/sizeof(commandsRequiringWritePermission[0]); ++ii) 
    {
        if (command == commandsRequiringWritePermission[ii]) 
        {
            reply("550 Can't do that in read-only mode.");
            return false;
        }
    }

    return true;
}

QString FtpControlConnection::stripFlagL(const QString &fileName)
{
    QString a = fileName.toUpper();
    if (a == "-L") 
    {
        return "";
    }
    if (a.startsWith("-L "))
    {
        return fileName.mid(3);
    }
    return fileName;
}

void FtpControlConnection::parseCommand(const QString &entireCommand, QString *command, QString *commandParameters)
{
    // Split parameters and command.
    int pos = entireCommand.indexOf(' ');
    if (-1 != pos)
    {
        *command = entireCommand.left(pos).trimmed().toUpper();
        *commandParameters = entireCommand.mid(pos+1).trimmed();
    } else 
    {
        *command = entireCommand.trimmed().toUpper();
    }
}

QString FtpControlConnection::toLocalPath(const QString &fileName) const
{
    QString localPath = fileName;

    // Some FTP clients send backslashes.
    localPath.replace('\\', '/');

    // If this is a relative path, we prepend the current directory.
    if (!localPath.startsWith('/')) 
    {
        localPath = m_currentDirectory + '/' + localPath;
    }

    // Evaluate all the ".." and ".", "/path/././to/dir/../.." becomes "/path".
    // Note we do this **before** prepending the root path, in order to avoid
    // "jailbreaking" out of the "chroot".
    QStringList components;
    foreach (const QString &component, localPath.split('/', QString::SkipEmptyParts)) 
    {
        if (component == "..")
        {
            if (!components.isEmpty()) 
            {
                components.pop_back();
            }
        } 
        else if (component != ".") 
        {
            components += component;
        }
    }

    // Prepend the root path.
    localPath = QDir::cleanPath(m_rootPath + '/' + components.join("/"));

    qDebug() << "to local path" << fileName << "->" << localPath;
    return localPath;
}

void FtpControlConnection::reply(const QString &replyCode)
{
    qDebug() << "reply" << replyCode;
    m_socket->write((replyCode + "\r\n").toUtf8());
}

void FtpControlConnection::processCommand(const QString &entireCommand)
{
    if (!entireCommand.contains("PASS"))
    {
        qDebug() << "command" << entireCommand;
    }
    else
    {
        qDebug() << "command \"PASS XXX\"";
    }

    QString command;
    QString commandParameters;
    parseCommand(entireCommand, &command, &commandParameters);

    if (!verifyAuthentication(command))
    {
        return;
    }

    if (!verifyWritePermission(command)) 
    {
        return;
    }

    if ("USER" == command) 
    {
        reply("331 User name OK, need password.");
    } 
    else if ("PASS" == command) 
    {
        pass(commandParameters);
    } 
    else if ("QUIT" == command) 
    {
        quit();
    } 
    else if ("AUTH" == command && "TLS" == commandParameters.toUpper())
    {
        auth();
    } 
    else if ("FEAT" == command) 
    {
        feat();
    } 
    else if ("PWD" == command || "XPWD" == command)
    {
        reply(QString("257 \"%1\"").arg(m_currentDirectory));
    } 
    else if ("CWD" == command) 
    {
        cwd(commandParameters);
    } 
    else if ("TYPE" == command) 
    {
        reply("200 Command okay.");
    } 
    else if ("PORT" == command)
    {
        port(commandParameters);
    } 
    else if ("PASV" == command)
    {
        pasv();
    } 
    else if ("LIST" == command) 
    {
        list(toLocalPath(stripFlagL(commandParameters)), false);
    } 
    else if ("RETR" == command) 
    {
        retr(toLocalPath(commandParameters));
    } 
    else if ("REST" == command) 
    {
        reply("350 Requested file action pending further information.");
    } 
    else if ("NLST" == command)
    {
        list(toLocalPath(stripFlagL(commandParameters)), true);
    } 
    else if ("SIZE" == command) 
    {
        size(toLocalPath(commandParameters));
    } 
    else if ("SYST" == command)
    {
        reply("215 Windows");
    } 
    else if ("PROT" == command) 
    {
        prot(commandParameters.toUpper());
    } 
    else if ("CDUP" == command) 
    {
        cdup();
    }
    else if ("OPTS" == command && "UTF8 ON" == commandParameters.toUpper())
    {
        reply("200 Command okay.");
    }
    else if ("PBSZ" == command && "0" == commandParameters.toUpper()) 
    {
        reply("200 Command okay.");
    }
    else if ("NOOP" == command) 
    {
        reply("200 Command okay.");
    } 
    else if ("STOR" == command)
    {
        stor(toLocalPath(commandParameters));
    }
    else if ("MKD" == command) 
    {
        mkd(toLocalPath(commandParameters));
    } 
    else if ("RMD" == command)
    {
        rmd(toLocalPath(commandParameters));
    } 
    else if ("DELE" == command) 
    {
        dele(toLocalPath(commandParameters));
    } 
    else if ("RNFR" == command)
    {
        reply("350 Requested file action pending further information.");
    }
    else if ("RNTO" == command) 
    {
        rnto(toLocalPath(commandParameters));
    } 
    else if ("APPE" == command) 
    {
        stor(toLocalPath(commandParameters), true);
    }
    else 
    {
        reply("502 Command not implemented.");
    }

    m_lastProcessedCommand = entireCommand;
}

void FtpControlConnection::startOrScheduleCommand(FtpCommand *ftpCommand)
{
    connect(ftpCommand, SIGNAL(reply(QString)), this, SLOT(reply(QString)));

    if (!m_dataConnection->setFtpCommand(ftpCommand))
    {
        delete ftpCommand;
        reply("425 Can't open data connection.");
        return;
    }
}

void FtpControlConnection::port(const QString &addressAndPort)
{
    // Example PORT command:
    // PORT h1,h2,h3,h4,p1,p2

    // Get IP and port.
    QRegExp exp("\\s*(\\d+,\\d+,\\d+,\\d+),(\\d+),(\\d+)");
    exp.indexIn(addressAndPort);
    QString hostName = exp.cap(1).replace(',', '.');
    int port = exp.cap(2).toInt() * 256 + exp.cap(3).toInt();
    m_dataConnection->scheduleConnectToHost(hostName, port, m_encryptDataConnection);
    reply("200 Command okay.");
}

void FtpControlConnection::pasv()
{
    int port = m_dataConnection->listen(m_encryptDataConnection);
    reply(QString("227 Entering Passive Mode (%1,%2,%3).").arg(m_socket->localAddress().toString().replace('.',',')).arg(port/256).arg(port%256));
}

void FtpControlConnection::list(const QString &dir, bool nameListOnly)
{
    startOrScheduleCommand(new FtpListCommand(this, dir, nameListOnly));
}

void FtpControlConnection::retr(const QString &fileName)
{
    startOrScheduleCommand(new FtpRetrCommand(this, fileName, seekTo()));
}

void FtpControlConnection::stor(const QString &fileName, bool appendMode)
{
    startOrScheduleCommand(new FtpStorCommand(this, fileName, appendMode, seekTo()));
}

void FtpControlConnection::cwd(const QString &dir)
{
    QFileInfo fi(toLocalPath(dir));
    if (fi.exists() && fi.isDir())
	{
        QFileInfo fi(dir);
        if (fi.isAbsolute())
		{
            m_currentDirectory = QDir::cleanPath(dir);
        } else 
		{
            m_currentDirectory = QDir::cleanPath(m_currentDirectory + '/' + dir);
        }
        reply("250 Requested file action okay, completed.");
    } 
	else 
	{
		if (!fi.isDir())
		{
			qWarning() << QStringLiteral("%1不是目录，cwd不执行").arg(dir);
			reply("550 CWD failed. directory not found.");
		}
		else
		{
			reply("550 Requested action not taken; file unavailable.");
		}
    }

	//if (!fi.exists())
	//{
	//	reply("550 Requested action not taken; file unavailable.");
	//	return;
	//}
	//if (!fi.isDir())
	//{
	//	reply("550 Requested action not taken; file unavailable.");
	//}
}

void FtpControlConnection::mkd(const QString &dir)
{
    if (QDir().mkdir(dir)) {
        reply(QString("257 \"%1\" created.").arg(dir));
    } else {
        reply("550 Requested action not taken; file unavailable.");
    }
}

void FtpControlConnection::rmd(const QString &dir)
{
    if (QDir().rmdir(dir)) {
        reply("250 Requested file action okay, completed.");
    } else {
        reply("550 Requested action not taken; file unavailable.");
    }
}

void FtpControlConnection::dele(const QString &fileName)
{
    if (QDir().remove(fileName)) {
        reply("250 Requested file action okay, completed.");
    } else {
        reply("550 Requested action not taken; file unavailable.");
    }
}

void FtpControlConnection::rnto(const QString &fileName)
{
    QString command;
    QString commandParameters;
    parseCommand(m_lastProcessedCommand, &command, &commandParameters);
    if ("RNFR" == command && QDir().rename(toLocalPath(commandParameters), fileName)) 
    {
        reply("250 Requested file action okay, completed.");
    } else
    {
        reply("550 Requested action not taken; file unavailable.");
    }
}

void FtpControlConnection::quit()
{
    reply("221 Quitting...");
    // If we have a running download or upload, we will wait until it's
    // finished before closing the control connection.
    if (m_dataConnection->ftpCommand()) 
    {
        connect(m_dataConnection->ftpCommand(), SIGNAL(destroyed()), this, SLOT(disconnectFromHost()));
    } else
    {
        disconnectFromHost();
    }
}

void FtpControlConnection::size(const QString &fileName)
{
    QFileInfo fi(fileName);
    if (!fi.exists() || fi.isDir()) 
    {
        reply("550 Requested action not taken; file unavailable.");
    }
    else
    {
        reply(QString("213 %1").arg(fi.size()));
    }
}

void FtpControlConnection::pass(const QString &password)
{
    QString command;
    QString commandParameters;
    parseCommand(m_lastProcessedCommand, &command, &commandParameters);
    if (this->m_password.isEmpty() || ("USER" == command && this->m_userName == commandParameters && this->m_password == password))
    {
        reply("230 You are logged in.");
        m_isLoggedIn = true;
    }
    else 
    {
        reply("530 User name or password was incorrect.");
    }
}

void FtpControlConnection::auth()
{
    reply("234 Initializing SSL connection.");
    SslServer::setLocalCertificateAndPrivateKey(m_socket);
    m_socket->startServerEncryption();
}

void FtpControlConnection::prot(const QString &protectionLevel)
{
    if ("C" == protectionLevel) 
    {
        m_encryptDataConnection = false;
    }
    else if ("P" == protectionLevel) 
    {
        m_encryptDataConnection = true;
    }
    else 
    {
        reply("502 Command not implemented.");
        return;
    }
    reply("200 Command okay.");
}

void FtpControlConnection::cdup()
{
    if ("/" == m_currentDirectory) 
    {
        reply("250 Requested file action okay, completed.");
    }
    else 
    {
        cwd("..");
    }
}

void FtpControlConnection::feat()
{
    // We only report that we support UTF8 file names, this is needed because
    // some clients will assume that we use ASCII otherwise, and will not
    // encode the filenames properly.
    m_socket->write(
        "211-Features:\r\n"
        " UTF8\r\n"
        "211 End\r\n"
    );
}

qint64 FtpControlConnection::seekTo()
{
    qint64 seekTo = 0;
    QString command;
    QString commandParameters;
    parseCommand(m_lastProcessedCommand, &command, &commandParameters);
    if ("REST" == command) 
    {
        QTextStream(commandParameters.toUtf8()) >> seekTo;
    }
    return seekTo;
}
