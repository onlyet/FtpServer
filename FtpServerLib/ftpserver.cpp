#include "ftpserver.h"
#include "ftpcontrolconnection.h"
#include "sslserver.h"

#include <QDebug>
#include <QNetworkInterface>
#include <QSslSocket>

FtpServer::FtpServer(QObject* parent, const QString& rootPath, const QString& ip, int port,
                     const QString& userName, const QString& password, bool readOnly,
                     bool onlyOneIpAllowed)
    : QObject(parent), m_ip(ip) {
    m_server = new SslServer(this);
    // In Qt4, QHostAddress::Any listens for IPv4 connections only, but as of
    // Qt5, it now listens on all available interfaces, and
    // QHostAddress::AnyIPv4 needs to be used if we want only IPv4 connections.
#if QT_VERSION >= 0x050000
    if (!m_ip.isEmpty()) {
        m_server->listen(QHostAddress(m_ip), port);
    } else {
        m_server->listen(QHostAddress::AnyIPv4, port);
    }
#else
    server->listen(QHostAddress::Any, port);
#endif
    connect(m_server, SIGNAL(newConnection()), this, SLOT(startNewControlConnection()));
    this->m_userName         = userName;
    this->m_password         = password;
    this->m_rootPath         = rootPath;
    this->m_readOnly         = readOnly;
    this->m_onlyOneIpAllowed = onlyOneIpAllowed;
}

bool FtpServer::isListening()
{
    return m_server->isListening();
}

int FtpServer::onlineIpNumber()
{
    QMutexLocker lock(&m_encounteredIpsMutex);
    return m_encounteredIps.size();
}

QString FtpServer::localIpv4()
{
    QList<QNetworkInterface> interfaceList = QNetworkInterface::allInterfaces();
    for(const QNetworkInterface& interfaceItem : interfaceList)
    {
        if (interfaceItem.flags().testFlag(QNetworkInterface::IsUp)
            && interfaceItem.flags().testFlag(QNetworkInterface::IsRunning)
            && interfaceItem.flags().testFlag(QNetworkInterface::CanBroadcast)
            && interfaceItem.flags().testFlag(QNetworkInterface::CanMulticast)
            && !interfaceItem.flags().testFlag(QNetworkInterface::IsLoopBack)
            && interfaceItem.hardwareAddress() != "00:50:56:C0:00:01"
            && interfaceItem.hardwareAddress() != "00:50:56:C0:00:08")
        {
            //遍历每一个接口信息
            qDebug() << "name:" << interfaceItem.humanReadableName();
            qDebug() << "type:" << interfaceItem.type();
            qDebug() << "id:" << interfaceItem.index();  //设备名称
            qDebug() << "Device:" << interfaceItem.name();  //设备名称
            qDebug() << "HardwareAddress:" << interfaceItem.hardwareAddress();  //获取硬件地址

            QList<QNetworkAddressEntry> addressEntryList = interfaceItem.addressEntries();
            for(const QNetworkAddressEntry& addressEntryItem : addressEntryList)
            {
                if (addressEntryItem.ip().protocol() == QAbstractSocket::IPv4Protocol)
                {
                    QString ip = addressEntryItem.ip().toString();
                    if (!ip.startsWith("169.254"))
                    {
                        //return ip;
                        qDebug() << "ip:" << ip;
                    }
                }
            }
        }
    }
    return QString("");
}

QString FtpServer::lanIp()
{
#if 0
    foreach(const QHostAddress & address, QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost)) {
            return address.toString();
        }
    }
    return "";
#endif
    QList<QNetworkInterface> interfaceList = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& interfaceItem : interfaceList) {
        if (interfaceItem.flags().testFlag(QNetworkInterface::IsUp)
            && interfaceItem.flags().testFlag(QNetworkInterface::IsRunning)
            && interfaceItem.flags().testFlag(QNetworkInterface::CanBroadcast)
            && interfaceItem.flags().testFlag(QNetworkInterface::CanMulticast)
            && !interfaceItem.flags().testFlag(QNetworkInterface::IsLoopBack)
            && interfaceItem.hardwareAddress() != "00:50:56:C0:00:01"
            && interfaceItem.hardwareAddress() != "00:50:56:C0:00:08") {
            QList<QNetworkAddressEntry> addressEntryList = interfaceItem.addressEntries();
            for (const QNetworkAddressEntry& addressEntryItem : addressEntryList) {
                if (addressEntryItem.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    return addressEntryItem.ip().toString();
                }
            }
        }
}
    return QString("");
#if 0
    return m_ip;
#endif
}

void FtpServer::startNewControlConnection()
{
    QSslSocket* socket = (QSslSocket*)m_server->nextPendingConnection();

    // If this is not a previously encountered IP emit the newPeerIp signal.
    QString peerIp = socket->peerAddress().toString();
    qDebug() << "connection from" << peerIp;

    QMutexLocker lock(&m_encounteredIpsMutex);
    {
        if (!m_encounteredIps.contains(peerIp)) 
        {
            // If we don't allow more than one IP for the client, we close
            // that connection.
            if (m_onlyOneIpAllowed && !m_encounteredIps.isEmpty()) 
            {
                delete socket;
                return;
            }

            emit newPeerIp(peerIp);
            m_encounteredIps.insert(peerIp);
        }
    }
    // Create a new FTP control connection on this socket.
    FtpControlConnection* conn = new FtpControlConnection(this, socket, m_rootPath, m_userName, m_password, m_readOnly);
    connect(conn, &FtpControlConnection::connectionClosed, [this](const QString& ip) 
        {
        QMutexLocker lock(&m_encounteredIpsMutex);
        m_encounteredIps.remove(ip);
        qDebug() << "ftp remove ip:" << ip;
        });
}
