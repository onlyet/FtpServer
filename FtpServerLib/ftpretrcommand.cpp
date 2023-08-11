#include "ftpretrcommand.h"
#include <QFile>
#include <QSslSocket>

FtpRetrCommand::FtpRetrCommand(QObject* parent, const QString& fileName, qint64 seekTo) :
    FtpCommand(parent)
{
    this->m_fileName = fileName;
    this->m_seekTo = seekTo;
    m_file = 0;
}

FtpRetrCommand::~FtpRetrCommand()
{
    if (m_started)
    {
        if (m_file && m_file->isOpen() && m_file->atEnd()) 
        {
            emit reply("226 Closing data connection.");
        }
        else 
        {
            emit reply("550 Requested action not taken; file unavailable.");
        }
    }
    qDebug() << "~FtpRetrCommand()";
}

void FtpRetrCommand::startImplementation()
{
    m_file = new QFile(m_fileName, this);
    if (!m_file->open(QIODevice::ReadOnly))
    {
        deleteLater();
        return;
    }
    emit reply("150 File status okay; about to open data connection.");
    if (m_seekTo)
    {
        m_file->seek(m_seekTo);
    }

    // For encryted SSL sockets, we need to use the encryptedBytesWritten()
    // signal, see the QSslSocket documentation to for reasons why.
    if (m_socket->isEncrypted()) 
    {
        connect(m_socket, SIGNAL(encryptedBytesWritten(qint64)), this, SLOT(refillSocketBuffer(qint64)));
    }
    else 
    {
        connect(m_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(refillSocketBuffer(qint64)));
    }

    refillSocketBuffer(128 * 1024);
}

void FtpRetrCommand::refillSocketBuffer(qint64 bytes)
{
    if (!m_file->atEnd())
    {
        m_socket->write(m_file->read(bytes));
    }
    else
    {
        m_socket->disconnectFromHost();
    }
}
