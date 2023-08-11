#ifndef FTPSERVER_H
#define FTPSERVER_H

#include <QObject>
#include <QSet>
#include <QMutex>
//#include "qftpserverlib_global.h"

class SslServer;

// The ftp server. Listens on a port, and starts a new control connection each
// time it gets connected.

class /*QFTPSERVERLIBSHARED_EXPORT*/ FtpServer : public QObject
{
    Q_OBJECT
public:
    explicit FtpServer(QObject* parent, const QString& rootPath, const QString& ip = "", int port = 21,
                       const QString& userName = QString(), const QString& password = QString(),
                       bool readOnly = false, bool onlyOneIpAllowed = false);

    // Whether or not the server is listening for incoming connections. If it
    // is not currently listening then there was an error - probably no
    // internet connection is available, or the port address might require root
    // priviledges (on Linux).
    bool isListening();

    // Get the LAN IP of the host, e.g. "192.168.1.10".
    static QString lanIp();

    int onlineIpNumber();

signals:
    // A connection from a new IP has been established. This signal is emitted
    // when the FTP server is connected by a new IP. The new IP will then be
    // stored and will not cause this FTP server instance to emit this signal
    // any more.
    void newPeerIp(const QString &ip);

protected:
    QString localIpv4();

private slots:
    // Called by the SSL server when we have received a new connection.
    void startNewControlConnection();

private:
    // If both username and password are empty, it means anonymous mode - any
    // username and password combination will be accepted.
    QString         m_userName;
    QString         m_password;

    // The root path is the virtual root directory of the FTP. It works like
    // chroot (see http://en.wikipedia.org/wiki/Chroot). The FTP server will
    // not access files or folders that are not in this folder or any of its
    // subfolders.
    QString         m_rootPath;

    // The SSL server listen for incoming connections.
    SslServer       *m_server;

    // All the IPs that this FTP server object has encountered in its lifetime.
    // See the signal newPeerIp.
    QSet<QString>   m_encounteredIps;
    QMutex          m_encounteredIpsMutex;

    // Whether or not the server is in read-only mode. In read-only mode the
    // server will not create, modify or delete any files or directories.
    bool            m_readOnly;

    // Causes the server to remember the first IP that connects to it, and then
    // refuse connections from any other IP. This makes sense because a mobile
    // phone is unlikely to be used from 2 places at once.
    bool            m_onlyOneIpAllowed;

    // 监听本地IP，非虚拟机IP，非169.254开头
    QString         m_ip;
};

#endif // FTPSERVER_H
