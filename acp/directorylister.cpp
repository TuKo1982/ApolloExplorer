#include "directorylister.h"

#include <iostream>
#include <QThread>
#include <QCoreApplication>

#define DEBUG1
#include "AEUtils.h"

DirectoryLister::DirectoryLister( QString ipAddressString, QString path, QObject *parent )
    : QObject{parent},
      m_Host( ipAddressString ),
      m_Path( path ),
      m_ProtocolHander( this )
{
    //As soon as we are connected, we will receive the volume list.  We need a slot for that
    connect( &m_ProtocolHander, &ProtocolHandler::volumeListSignal, this, &DirectoryLister::onVolumeListSlot );
}

QStringList DirectoryLister::getDirectoryList()
{
    QStringList dirContents;
    m_Error.clear();
    m_Connected = m_ConnectionFailed = m_ListingFailed = m_ListingReceived = false;
    m_Listing.clear();

    //All handlers write to members and use 'this' as context, so a signal that
    //arrives after we returned (e.g. the disconnect when the socket is torn down)
    //can never write into a stack frame that no longer exists.
    QObject::connect( &m_ProtocolHander, &ProtocolHandler::serverVersionSignal, this, [this]( quint8, quint8, quint8 ) {
        m_Connected = true;
    });
    QObject::connect( &m_ProtocolHander, &ProtocolHandler::disconnectedFromHostSignal, this, [this]() {
        m_ConnectionFailed = true;
    });
    QObject::connect( &m_ProtocolHander, &ProtocolHandler::serverClosedConnectionSignal, this, [this]( QString reason ) {
        m_ConnectionFailed = true;
        m_Error = reason;
    });
    QObject::connect( &m_ProtocolHander, &ProtocolHandler::newDirectoryListingSignal, this, [this]( QSharedPointer<DirectoryListing> listing ) {
        m_ListingReceived = true;
        m_Listing = listing;
    });
    //The server answers PMT_FAILED when the path doesn't exist; without this we waited forever
    QObject::connect( &m_ProtocolHander, &ProtocolHandler::failedWithReasonSignal, this, [this]( QString reason ) {
        m_ListingFailed = true;
        m_Error = reason;
    });

    //Connect to the host
    DBGLOG << "Connecting to host";
    m_ProtocolHander.onConnectToHostRequestedSlot( QHostAddress( m_Host ), MAIN_LISTEN_PORTNUMBER );

    //Perform the wait on the
    for( int i=0; i< 1000 && !m_Connected && !m_ConnectionFailed; i++)
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    //Did we connected?
    if( !m_Connected )
    {
        if( m_Error.isEmpty() )
            m_Error = "Couldn't connect to host.";
        return dirContents;
    }

    DBGLOG << "Connected.";

    //So now we are connected
    //Get the directory in question
    if( m_Path.length() == 0 )
    {
        //Ok, no path was specified, so get the drive list
        m_ProtocolHander.onGetVolumeListSlot();

        //If we don't have the drive list, lets cycle a few times and see what we get
        for( int i=0; i < 1000 && !m_ConnectionFailed; i++ )
        {
            //Do we have the list of drives already?
            if( m_Volumes.size() > 0 )
            {
                //Ok we have the list of drives.
                //Lets reproduce this as strings
                for( const QSharedPointer<DiskVolume>& nextVolume : m_Volumes )
                {
                    dirContents.append( nextVolume->getName() + ":" );
                }

                //Now return the list
                return dirContents;
            }

            //Wait a bit
            QThread::msleep( 10 );
            QCoreApplication::processEvents();
        }

        if( m_Error.isEmpty() )
            m_Error = m_ConnectionFailed ? "Connection to host lost." : "No volume list received from host.";
        return dirContents;
    }

    //Check the user supplied path
    m_ProtocolHander.onGetDirectorySlot( m_Path );

    //Perform the wait (at most 30s - a large directory is still well within that)
    for( int i = 0; i < 3000 && !m_ListingReceived && !m_ListingFailed && !m_ConnectionFailed; i++ )
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    if( !m_ListingReceived || m_Listing.isNull() )
    {
        if( m_Error.isEmpty() )
            m_Error = m_ConnectionFailed ? "Connection to host lost." : "No directory listing received from host.";
        return dirContents;
    }
    QSharedPointer<DirectoryListing> newListing = m_Listing;

    //Now process the list we got
    for( auto iter = newListing->Entries().begin(); iter != newListing->Entries().end(); iter++ )
    {
        QSharedPointer<DirectoryListing> entry = *iter;
        QString entryPath = entry->Path();
        if( entry->isDirectory() ) entryPath += "/";
        dirContents.append( entryPath );
    }

    return dirContents;
}

void DirectoryLister::onVolumeListSlot(QList<QSharedPointer<DiskVolume> > volumes)
{
    m_Volumes = volumes;
}
