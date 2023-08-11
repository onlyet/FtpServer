#include "dataconnection.h"
#include "sslserver.h"
#include "ftpcommand.h"
#include <QSslSocket>

DataConnection::DataConnection(QObject *parent) :
    QObject(parent)
{
    m_server = new SslServer(this);
    connect(m_server, SIGNAL(newConnection()), this, SLOT(newConnection()));
    m_socket = 0;
    m_isSocketReady = false;
    m_isWaitingForFtpCommand = false;
    qDebug() << "DataConnection()";
}

DataConnection::~DataConnection()
{
    qDebug() << "~DataConnection()";
}

void DataConnection::scheduleConnectToHost(const QString &hostName, int port, bool encrypt)
{
    this->m_encrypt = encrypt;
    delete m_socket;
    this->m_hostName = hostName;
    this->m_port = port;
    m_isSocketReady = false;
    m_isWaitingForFtpCommand = true;
    m_isActiveConnection = true;
}

int DataConnection::listen(bool encrypt)
{
    this->m_encrypt = encrypt;
    delete m_socket;
    m_socket = 0;
    // 注释该行，因为FtpCommand会在具体的命令完成的时候析构
    //delete m_command; 
    m_command = nullptr;
    m_isSocketReady = false;
    m_isWaitingForFtpCommand = true;
    m_isActiveConnection = false;
    m_server->close();
    m_server->listen();
    return m_server->serverPort();
}

bool DataConnection::setFtpCommand(FtpCommand *command)
{
    if (!m_isWaitingForFtpCommand) 
    {
        return false;
    }
    m_isWaitingForFtpCommand = false;
    this->m_command = command;
    command->setParent(this);
    qDebug() << "setFtpCommand";

    if (m_isActiveConnection) 
    {
        m_socket = new QSslSocket(this);
        connect(m_socket, SIGNAL(connected()), SLOT(connected()));
        m_socket->connectToHost(m_hostName, m_port);
    } 
    else 
    {
        startFtpCommand();
    }
    return true;
}

FtpCommand *DataConnection::ftpCommand()
{
    if (m_isSocketReady)
    {
        return m_command;
    }
    return 0;
}

void DataConnection::newConnection()
{
    m_socket = (QSslSocket *) m_server->nextPendingConnection();
    // 不再监听，不接收新连接
    m_server->close();
    if (m_encrypt)
    {
        connect(m_socket, SIGNAL(encrypted()), this, SLOT(encrypted()));
        SslServer::setLocalCertificateAndPrivateKey(m_socket);
        m_socket->startServerEncryption();
    }
    else 
    {
        encrypted();
    }
}

void DataConnection::encrypted()
{
    m_isSocketReady = true;
    startFtpCommand();
}

void DataConnection::connected()
{
    if (m_encrypt) 
    {
        connect(m_socket, SIGNAL(encrypted()), this, SLOT(encrypted()));
        SslServer::setLocalCertificateAndPrivateKey(m_socket);
        m_socket->startServerEncryption();
    } 
    else 
    {
        encrypted();
    }
}

void DataConnection::startFtpCommand()
{
    if (m_command && m_isSocketReady) 
    {
        m_command->start(m_socket);
        m_socket = 0;
    }
}
