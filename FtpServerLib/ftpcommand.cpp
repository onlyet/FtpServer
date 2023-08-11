#include "ftpcommand.h"

#include <QSslSocket>

FtpCommand::FtpCommand(QObject* parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_started(false) {
}

FtpCommand::~FtpCommand()
{
    qDebug() << "~FtpCommand()";
}

void FtpCommand::start(QSslSocket *socket)
{
    m_started = true;
    this->m_socket = socket;
    socket->setParent(this);
    connect(socket, SIGNAL(disconnected()), this, SLOT(deleteLater()));
    startImplementation();
}
