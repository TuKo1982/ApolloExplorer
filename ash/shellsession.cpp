#include "shellsession.h"

#include <iostream>
#include <QThread>
#include <QCoreApplication>
#include <QtEndian>
#include <QElapsedTimer>

#define DEBUG 0
#include "AEUtils.h"
#include "messagepool.h"

ShellSession::ShellSession( QString ipAddressString, QObject *parent )
    : QObject{ parent },
      m_Host( ipAddressString ),
      m_ProtocolHandler( this ),
      m_CommandDone( false ),
      m_ConnectionLost( false ),
      m_TimeoutSeconds( 0 ),
      m_ReturnCode( 0 ),
      m_CompletionDone( false )
{
    connect( &m_ProtocolHandler, &ProtocolHandler::shellOutputSignal,
             this, &ShellSession::onShellOutputSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::shellDoneSignal,
             this, &ShellSession::onShellDoneSlot, Qt::DirectConnection );
    connect( &m_ProtocolHandler, &ProtocolHandler::shellCompleteRspSignal,
             this, &ShellSession::onShellCompleteRspSlot, Qt::DirectConnection );

    // Without these, a server that rejects the command or drops the connection
    // leaves execute() waiting forever for a PMT_SHELL_DONE that never comes.
    connect( &m_ProtocolHandler, &ProtocolHandler::serverClosedConnectionSignal,
             this, &ShellSession::onServerClosedConnectionSlot, Qt::DirectConnection );
    connect( &m_ProtocolHandler, &ProtocolHandler::disconnectedFromHostSignal,
             this, &ShellSession::onDisconnectedSlot, Qt::DirectConnection );
}

bool ShellSession::connectToHost()
{
    m_ProtocolHandler.onConnectToHostRequestedSlot( QHostAddress( m_Host ), MAIN_LISTEN_PORTNUMBER );

    bool connected = false;
    QObject::connect( &m_ProtocolHandler, &ProtocolHandler::connectedToHostSignal, [&]() {
        connected = true;
    });

    for( int i = 0; i < 1000; i++ )
    {
        if( connected ) break;
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    return connected;
}

void ShellSession::disconnectFromHost()
{
    m_ProtocolHandler.onDisconnectFromHostRequestedSlot();
    QThread::msleep( 200 );
    QCoreApplication::processEvents();
}

int ShellSession::execute( const QString &command )
{
    m_CommandDone = false;
    m_ReturnCode = 0;

    if( m_ConnectionLost )
    {
        std::cerr << "ash: not connected to " << m_Host.toStdString() << std::endl;
        return ASH_EXIT_CONNECTION_LOST;
    }

    QByteArray encodedCommand = command.toLatin1();
    int cmdLen = encodedCommand.size() + 1;
    int msgLen = sizeof( ProtocolMessage_t ) + cmdLen;

    QByteArray msgBuf( msgLen, 0 );
    ProtocolMessage_ShellExec_t *msg =
        reinterpret_cast<ProtocolMessage_ShellExec_t*>( msgBuf.data() );
    msg->header.token  = MAGIC_TOKEN;
    msg->header.type   = PMT_SHELL_EXEC;
    msg->header.length = msgLen;
    memcpy( msg->command, encodedCommand.constData(), cmdLen );

    m_ProtocolHandler.sendMessage( msg );

    QElapsedTimer timer;
    timer.start();
    bool timedOut = false;

    while( !m_CommandDone && !m_ConnectionLost )
    {
        for( const QByteArray &chunk : m_PendingOutput )
            std::cout.write( chunk.constData(), chunk.size() );
        m_PendingOutput.clear();

        if( m_TimeoutSeconds > 0 && timer.elapsed() >= qint64( m_TimeoutSeconds ) * 1000 )
        {
            timedOut = true;
            break;
        }

        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    for( const QByteArray &chunk : m_PendingOutput )
        std::cout.write( chunk.constData(), chunk.size() );
    m_PendingOutput.clear();
    std::cout.flush();

    if( m_CommandDone )
        return m_ReturnCode;

    if( timedOut )
    {
        // The command may still be running on the Amiga; its output is abandoned.
        std::cerr << "ash: no reply from " << m_Host.toStdString() << " within "
                  << m_TimeoutSeconds << "s, giving up." << std::endl;
        m_ConnectionLost = true;
        return ASH_EXIT_TIMEOUT;
    }

    std::cerr << "ash: connection to " << m_Host.toStdString() << " closed before the command finished";
    if( !m_CloseReason.isEmpty() )
        std::cerr << ": " << m_CloseReason.toStdString();
    std::cerr << std::endl;
    return ASH_EXIT_CONNECTION_LOST;
}

QStringList ShellSession::requestCompletion( const QString &partialToken )
{
    m_CompletionDone = false;
    m_Completions.clear();

    QByteArray encoded = partialToken.toLatin1();
    QByteArray msgBuf( sizeof( ProtocolMessage_ShellCompleteReq_t ), 0 );
    ProtocolMessage_ShellCompleteReq_t *msg =
        reinterpret_cast<ProtocolMessage_ShellCompleteReq_t*>( msgBuf.data() );
    msg->header.token  = MAGIC_TOKEN;
    msg->header.type   = PMT_SHELL_COMPLETE_REQ;
    msg->header.length = sizeof( ProtocolMessage_ShellCompleteReq_t );
    strncpy( msg->partial, encoded.constData(), sizeof( msg->partial ) - 1 );

    m_ProtocolHandler.sendMessage( msg );

    for( int i = 0; i < 500 && !m_CompletionDone && !m_ConnectionLost; i++ )
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    return m_Completions;
}

void ShellSession::onShellOutputSlot( quint32 bytesContained, QByteArray data )
{
    Q_UNUSED( bytesContained )
    m_PendingOutput.append( data );
}

void ShellSession::onShellDoneSlot( int returnCode, QString currentDir )
{
    m_ReturnCode = returnCode;
    m_CurrentDir = currentDir;
    m_CommandDone = true;
}

void ShellSession::onShellCompleteRspSlot( QStringList completions )
{
    m_Completions = completions;
    m_CompletionDone = true;
}

void ShellSession::onServerClosedConnectionSlot( QString reason )
{
    m_CloseReason = reason;
    m_ConnectionLost = true;
}

void ShellSession::onDisconnectedSlot()
{
    m_ConnectionLost = true;
}
