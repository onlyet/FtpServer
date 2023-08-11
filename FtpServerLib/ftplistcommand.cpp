#include "ftplistcommand.h"

#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTimer>
#include <QSslSocket>

FtpListCommand::FtpListCommand(QObject* parent, const QString& fileName, bool nameListOnly)
    : FtpCommand(parent)
    , m_timer(nullptr)
    , m_list(nullptr)
    , m_index(0) {
    this->m_listDirectory = fileName;
    this->m_nameListOnly = nameListOnly;
}

FtpListCommand::~FtpListCommand()
{
    if (m_started) 
    {
        emit reply("226 Closing data connection.");
    }
    qDebug() << "~FtpListCommand()";
}

void FtpListCommand::startImplementation()
{
    QFileInfo info(m_listDirectory);

    if (!info.isReadable())
    {
        emit reply("425 File or directory is not readable or doesn't exist.");
        m_socket->disconnectFromHost();
        return;
    }

    emit reply("150 File status okay; about to open data connection.");

    m_index = 0;
    m_list = new QFileInfoList;
    if (!info.isDir())
    {
        *m_list = (QFileInfoList() << info);
    }
    else
    {
        // ls -l
        *m_list = QDir(m_listDirectory).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    }

    // Start the timer.
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(listNextBatch()));
    m_timer->start(0);
}

QString padded(QString s, int n)
{
    while (s.size() < n) 
    {
        s = ' ' + s;
    }
    return s;
}

QString FtpListCommand::fileListingString(const QFileInfo& fi)
{
    // This is how the returned list looks. It is like what is returned by
    // 'ls -l':
    // drwxr-xr-x    9 ftp      ftp          4096 Nov 17  2009 pub

    QString line;
    if (!m_nameListOnly) 
    {
        // Directory/symlink/file.
        if (fi.isSymLink())
        {
            line += 'l';
        }
        else if (fi.isDir()) 
        {
            line += 'd';
        }
        else 
        {
            line += '-';
        }

        // Permissions.
        QFile::Permissions p = fi.permissions();
        line += (p & QFile::ReadOwner) ? 'r' : '-';
        line += (p & QFile::WriteOwner) ? 'w' : '-';
        line += (p & QFile::ExeOwner) ? 'x' : '-';
        line += (p & QFile::ReadGroup) ? 'r' : '-';
        line += (p & QFile::WriteGroup) ? 'w' : '-';
        line += (p & QFile::ExeGroup) ? 'x' : '-';
        line += (p & QFile::ReadOther) ? 'r' : '-';
        line += (p & QFile::WriteOther) ? 'w' : '-';
        line += (p & QFile::ExeOther) ? 'x' : '-';

        line += " 1";

        // Owner/group.
        QString owner = fi.owner();
        if (owner.isEmpty())
        {
            owner = "ftp";
        }
        QString group = fi.group();
        if (group.isEmpty())
        {
            group = "ftp";
        }
        line += ' ' + owner + ' ' + group;

        // File size.
        line += ' ' + padded(QString::number(fi.size()), 14);

        // Last modified - note we **must** use english locale, otherwise FTP clients won't understand us.
        QLocale locale(QLocale::English);
        QDateTime lm = fi.lastModified();
        if (lm.date().year() != QDate::currentDate().year()) {
            line += ' ' + locale.toString(lm.date(), "MMM dd  yyyy");
        }
        else {
            line += ' ' + locale.toString(lm.date(), "MMM dd") + ' ' + locale.toString(lm.time(), "hh:mm");
        }
    }
    line += ' ' + fi.fileName();
    line += "\r\n";
    return line;
}

// 写完10行后继续调用listNextBatch。返回时间循环的目的是什么？
// list完则关闭数据连接
void FtpListCommand::listNextBatch()
{
    // List next 10 items.
    int stop = qMin(m_index + 10, m_list->size());
    while (m_index < stop) 
    {
        QString line = fileListingString(m_list->at(m_index));
        m_socket->write(line.toUtf8());
        qDebug() << "write line toUtf8: " << line.toUtf8();
        m_index++;
    }

    // If all files have been listed, then finish.
    if (m_list->size() == stop)
    {
        delete m_list;
        m_timer->stop();
        m_socket->disconnectFromHost();
    }
}
