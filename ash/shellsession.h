#ifndef SHELLSESSION_H
#define SHELLSESSION_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QStringList>
#include "protocolhandler.h"

// Exit codes returned by execute() when the command never completed on the Amiga.
// Chosen above the AmigaDOS range (0/5/10/20) so callers can tell them apart.
#define ASH_EXIT_TIMEOUT            124
#define ASH_EXIT_CONNECTION_LOST    125

class ShellSession : public QObject
{
    Q_OBJECT

public:
    explicit ShellSession( QString ipAddressString, QObject *parent = nullptr );

    bool connectToHost();
    void disconnectFromHost();
    int execute( const QString &command );
    QStringList requestCompletion( const QString &partialToken );
    QString currentDir() const { return m_CurrentDir; }
    bool isConnected() const { return !m_ConnectionLost; }
    void setTimeout( int seconds ) { m_TimeoutSeconds = seconds; }

private slots:
    void onShellOutputSlot( quint32 bytesContained, QByteArray data );
    void onShellDoneSlot( int returnCode, QString currentDir );
    void onShellCompleteRspSlot( QStringList completions );
    void onServerClosedConnectionSlot( QString reason );
    void onDisconnectedSlot();

private:
    QString m_Host;
    ProtocolHandler m_ProtocolHandler;
    bool m_CommandDone;
    bool m_ConnectionLost;
    QString m_CloseReason;
    int m_TimeoutSeconds;           // 0 = wait forever
    int m_ReturnCode;
    QString m_CurrentDir;
    QList<QByteArray> m_PendingOutput;
    bool m_CompletionDone;
    QStringList m_Completions;
};

#endif // SHELLSESSION_H
