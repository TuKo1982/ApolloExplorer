#ifndef DIRECTORYLISTER_H
#define DIRECTORYLISTER_H

#include <QObject>
#include <QSharedPointer>

#include "amigahost.h"
#include "protocolhandler.h"

class DirectoryLister : public QObject
{
    Q_OBJECT
public:
    explicit DirectoryLister( QString ipAddressString, QString path, QObject *parent = nullptr);
    QStringList getDirectoryList();
    QString errorMessage() const { return m_Error; }     //Empty when the last listing succeeded

public slots:
    void onVolumeListSlot( QList<QSharedPointer<DiskVolume>> volumes );

private:
    QString m_Host;
    QString m_Path;
    ProtocolHandler m_ProtocolHander;
    QList<QSharedPointer<DiskVolume>> m_Volumes;
    QString m_Error;
    bool m_Connected = false;
    bool m_ConnectionFailed = false;
    bool m_ListingFailed = false;
    bool m_ListingReceived = false;
    QSharedPointer<DirectoryListing> m_Listing;
};

#endif // DIRECTORYLISTER_H
