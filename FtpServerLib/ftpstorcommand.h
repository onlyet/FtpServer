#ifndef FTPSTORCOMMAND_H
#define FTPSTORCOMMAND_H

#include "ftpcommand.h"

class QFile;

// Implements the STOR and APPE commands. Used to upload files to the ftp
// server.

class FtpStorCommand : public FtpCommand
{
    Q_OBJECT
public:
    explicit FtpStorCommand(QObject *parent, const QString &fileName, bool appendMode = false, qint64 seekTo = 0);
    ~FtpStorCommand();

private slots:
    void acceptNextBlock();

private:
    void startImplementation();

    QString         m_fileName;
    QFile           *m_file;
    bool            m_appendMode;
    qint64          m_seekTo;
    bool            m_success;
};

#endif // FTPSTORCOMMAND_H
