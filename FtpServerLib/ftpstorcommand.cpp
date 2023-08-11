#include "ftpstorcommand.h"
#include <QFile>
#include <QSslSocket>

FtpStorCommand::FtpStorCommand(QObject* parent, const QString& fileName, bool appendMode, qint64 seekTo) :
    FtpCommand(parent)
{
    this->m_fileName = fileName;
    this->m_appendMode = appendMode;
    m_file = 0;
    this->m_seekTo = seekTo;
    m_success = false;
}

FtpStorCommand::~FtpStorCommand()
{
    if (m_started)
    {
        if (m_success)
        {
            emit reply("226 Closing data connection.");
        }
        else
        {
            emit reply("451 Requested action aborted: local error in processing.");
        }
    }
    qDebug() << "~FtpStorCommand()";
}

void FtpStorCommand::startImplementation()
{
    m_file = new QFile(m_fileName, this);
    if (!m_file->open(m_appendMode ? QIODevice::Append : QIODevice::WriteOnly)) 
    {
        deleteLater();
        return;
    }
    m_success = true;
    emit reply("150 File status okay; about to open data connection.");
    if (m_seekTo) 
    {
        m_file->seek(m_seekTo);
    }
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(acceptNextBlock()));
}

void FtpStorCommand::acceptNextBlock()
{
    const QByteArray& bytes = m_socket->readAll();
    int bytesWritten = m_file->write(bytes);
    if (bytesWritten != bytes.size()) 
    {
        emit reply("451 Requested action aborted. Could not write data to file.");
        deleteLater();
    }
}
