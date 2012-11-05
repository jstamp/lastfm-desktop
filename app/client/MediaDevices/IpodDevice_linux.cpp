/*
   Copyright 2005-2010 Last.fm Ltd.
   Portions Copyright (c) 2012 Matěj Laitl <matej@laitl.cz>

      - Primarily authored by Max Howell, Jono Cole and Doug Mansell

   This file is part of the Last.fm Desktop Application Suite.

   lastfm-desktop is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   lastfm-desktop is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with lastfm-desktop.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "IpodDevice_linux.h"
#include "lib/unicorn/QMessageBoxBuilder.h"
#include "lib/unicorn/UnicornSession.h"

#include <QSqlError>
#include <QSqlQuery>

#define DB_NAME "playcounts.db"

extern "C"
{
    #include <sys/utsname.h>
    #include <gpod/itdb.h>
    #include <glib.h>
}


// =============================== //

// This section is taken from Amarok's IpodDeviceHelper.cpp

static bool
firewireGuidNeeded( const Itdb_IpodGeneration &generation )
{
    switch( generation )
    {
        // taken from libgpod itdb_device.c itdb_device_get_checksum_type()
        // not nice, but should not change, no new devices use hash58
        case ITDB_IPOD_GENERATION_CLASSIC_1:
        case ITDB_IPOD_GENERATION_CLASSIC_2:
        case ITDB_IPOD_GENERATION_CLASSIC_3:
        case ITDB_IPOD_GENERATION_NANO_3:
        case ITDB_IPOD_GENERATION_NANO_4:
            return true; // ITDB_CHECKSUM_HASH58
        default:
            break;
    }
    return false;
}

static bool
hashInfoNeeded( const Itdb_IpodGeneration &generation )
{
    switch( generation )
    {
        // taken from libgpod itdb_device.c itdb_device_get_checksum_type()
        // not nice, but should not change, current devices need libhashab
        case ITDB_IPOD_GENERATION_NANO_5:
        case ITDB_IPOD_GENERATION_TOUCH_1:
        case ITDB_IPOD_GENERATION_TOUCH_2:
        case ITDB_IPOD_GENERATION_TOUCH_3:
        case ITDB_IPOD_GENERATION_IPHONE_1:
        case ITDB_IPOD_GENERATION_IPHONE_2:
        case ITDB_IPOD_GENERATION_IPHONE_3:
            return true; // ITDB_CHECKSUM_HASH72
        default:
            break;
    }
    return false;
}

/**
 * Returns true if file @param relFilename is found, readable and nonempty.
 * Searches in @param mountPoint /iPod_Control/Device/
 */
static bool
fileFound( const QString &mountPoint, const QString &relFilename )
{
    gchar *controlDir = itdb_get_device_dir( QFile::encodeName( mountPoint ) );
    if( !controlDir )
        return false;
    QString absFilename = QString( "%1/%2" ).arg( QFile::decodeName( controlDir ) )
                                            .arg( relFilename );
    g_free( controlDir );

    QFileInfo fileInfo( absFilename );
    return fileInfo.isReadable() && fileInfo.size() > 0;
}


// =============================== //


class IpodDeviceLinuxPrivate
{
public:
    IpodDeviceLinuxPrivate();
    bool initIpod();
    QSqlDatabase database() const;
    void commit( Itdb_Track* iTrack );
    void setTrackInfo( Track& lstTrack, Itdb_Track* iTrack, int scrobbleCount );
    uint previousPlayCount( Itdb_Track* iTrack ) const;

    Itdb_iTunesDB* itdb;
    bool writable;
    QSqlDatabase scrobblesDb;
    QList<Track> tracksToScrobble;
    QString sysname;
    QString deviceId;
    QString mountPath;
    QString ipodModel;
    IpodDeviceLinux::Error error;
};


IpodDeviceLinuxPrivate::IpodDeviceLinuxPrivate()
{
    itdb = NULL;
    writable = true;
    error = IpodDeviceLinux::NoError;
    struct utsname unameData;
    if ( uname( &unameData ) < 0 )
        sysname = "Unix";
    else
        sysname = unameData.sysname;
}

void
IpodDeviceLinuxPrivate::setTrackInfo( Track& lstTrack, Itdb_Track* iTrack, int scrobbleCount )
{
    MutableTrack( lstTrack ).setArtist( QString::fromUtf8( iTrack->artist ) );
    MutableTrack( lstTrack ).setAlbumArtist( QString::fromUtf8( iTrack->albumartist ) );
    MutableTrack( lstTrack ).setAlbum( QString::fromUtf8( iTrack->album ) );
    MutableTrack( lstTrack ).setTitle( QString::fromUtf8( iTrack->title ) );
    MutableTrack( lstTrack ).setSource( Track::MediaDevice );
    if ( iTrack->mediatype & ITDB_MEDIATYPE_PODCAST )
        MutableTrack( lstTrack ).setPodcast( true );

    bool video = (iTrack->mediatype & (ITDB_MEDIATYPE_MOVIE | ITDB_MEDIATYPE_TVSHOW)) &&
                 !(iTrack->mediatype & ITDB_MEDIATYPE_MUSICVIDEO);
    MutableTrack( lstTrack ).setVideo( video );

    QDateTime t;
    t.setTime_t( iTrack->time_played );
    MutableTrack( lstTrack ).setTimeStamp( t );
    MutableTrack( lstTrack ).setDuration( iTrack->tracklen / 1000 ); // set duration in seconds

    // FIXME: path?

    MutableTrack( lstTrack ).setExtra( "uniqueId", lstTrack.artist() + '\t' + lstTrack.title() + '\t' + lstTrack.album() );
    // FIXME: real dev name?
    MutableTrack( lstTrack ).setExtra( "deviceId", "ipod-" + sysname );
    MutableTrack( lstTrack ).setExtra( "playCount", QString::number( scrobbleCount ) );
}

void
IpodDeviceLinuxPrivate::commit( Itdb_Track* iTrack )
{
    QSqlQuery query( scrobblesDb );
    QString queryStr = "INSERT OR REPLACE INTO itunes_db ( persistent_id, path, play_count ) "
                       "VALUES ( ?, ?, ? );";
    query.prepare( queryStr );

    // using dbid because iTrack->id doesn't stay consistent between iPod syncs
    query.addBindValue( QString::number(iTrack->dbid, 16).toUpper() );
    // Also use the path like on Windows. This might prove useful if we want to
    // deal with duplicate track titles.
    query.addBindValue( QString( "%1\t%2\t%3" ).arg(iTrack->artist).arg(iTrack->title).arg(iTrack->album) );
    query.addBindValue( iTrack->playcount );

    query.exec();
    if( query.lastError().type() != QSqlError::NoError )
        qWarning() << query.lastError().text();
}

uint
IpodDeviceLinuxPrivate::previousPlayCount( Itdb_Track* track ) const
{
    QSqlQuery query( scrobblesDb );
    QString sql = "SELECT play_count FROM itunes_db WHERE persistent_id=\""
        + QString::number( track->dbid, 16 ).toUpper() + "\";";

    query.exec( sql );

    if( query.next() )
        return query.value( 0 ).toUInt();
    return 0;
}

QSqlDatabase
IpodDeviceLinuxPrivate::database() const
{
    QString const name = QString( DB_NAME ) + "_" + deviceId;
    QSqlDatabase db = QSqlDatabase::database( name );

    if ( !db.isValid() )
    {
        db = QSqlDatabase::addDatabase( "QSQLITE", name );

        if ( !deviceId.isEmpty() )
        {
            QDir dbDir = lastfm::dir::runtimeData().filePath( "devices/" + ipodModel + "/" + deviceId );
            dbDir.mkpath( "." );
            db.setDatabaseName( dbDir.filePath( DB_NAME ) );
            db.open();
        }
    }

    if( !db.tables().contains( "itunes_db" ) )
    {
        QSqlQuery q( db );
        bool b = q.exec( "CREATE TABLE itunes_db ( "
                         "persistent_id VARCHAR( 32 ) PRIMARY KEY, "
                         "path TEXT, "
                         "play_count INTEGER );" );
        q.exec( "CREATE INDEX persistent_id_idx ON itunes_db ( persistent_id );" );
        if ( !b )
            qWarning() << q.lastError().text();
    }

    return db;
}

bool
IpodDeviceLinuxPrivate::initIpod()
{
    QByteArray _mp = QFile::encodeName( mountPath );
    const char* mp = _mp.data();

    GError* err = 0;
    itdb = itdb_parse( mp, &err );
    if ( err )
    {
        qDebug() << "Error reading the iPod database:" << err->message;
        g_error_free( err );
        return false;
    }

    const Itdb_IpodInfo * device = itdb_device_get_ipod_info( itdb->device );
    if ( !device || device->ipod_model == ITDB_IPOD_MODEL_INVALID || device->ipod_model == ITDB_IPOD_MODEL_UNKNOWN )
    {
        itdb_free( itdb );
        itdb = NULL;
        qDebug() << "Unknown iPod model";
        return false;
    }

    ipodModel = QString::fromUtf8( itdb_info_get_ipod_model_name_string( device->ipod_model ) ).split( " " ).first().toLower();
    QString gen = QString::fromUtf8( itdb_info_get_ipod_generation_string( device->ipod_generation ) );

    // This little section adapted from Amarok's IpodDeviceHelper.cpp
    if( firewireGuidNeeded( device->ipod_generation ) )
    {
        // okay FireWireGUID may be in plain SysInfo, too, but it's hard to check and
        // error-prone so we just require SysInfoExtended which is machine-generated
        const QString sysInfoExtended( "SysInfoExtended" );
        bool sysInfoExtendedExists = fileFound( mp, sysInfoExtended );
        if( !sysInfoExtendedExists )
        {
            QString message = QString( "%1 family needs %2 file to generate correct database checksum." )
                                .arg( gen ).arg(sysInfoExtended );
            qDebug() << message;
            writable = false;
        }
    }
    if( hashInfoNeeded( device->ipod_generation ) )
    {
        const QString hashInfo( "HashInfo" );
        bool hashInfoExists = fileFound( mp, hashInfo );
        if( !hashInfoExists )
        {
            QString message = QString ( "%1 family needs %2 file to generate correct database checksum." )
                                .arg( gen ).arg( hashInfo );
            qDebug() << message;
            writable = false;
        }
    }

    // A manual scrobble...
    if ( deviceId.isEmpty() )
    {
        deviceId = itdb_device_get_sysinfo( itdb->device, "FirewireGuid" );
    }

    return true;
}


// =============================== //


IpodDeviceLinux::IpodDeviceLinux( const QString& mountPath, const QString& deviceId )
    : d( new IpodDeviceLinuxPrivate() )
{
    d->mountPath = mountPath;
    d->deviceId = deviceId;
}


IpodDeviceLinux::~IpodDeviceLinux()
{
    if ( d->itdb )
    {
        itdb_free( d->itdb );
    }
    if ( d->scrobblesDb.isValid() )
        d->scrobblesDb.close();
    delete d;
}

void
IpodDeviceLinux::fetchTracksToScrobble()
{
    QTime time;
    time.start();
    qDebug() << QString( "Scrobbling device \'%1\' at mount path \'%2\'" ).arg( d->deviceId ).arg( d->mountPath );
    if ( !d->initIpod() )
    {
        d->error = AccessError;
        emit errorOccurred( d->error );
        return;
    }
    
    emit calculatingScrobbles();

    d->scrobblesDb = d->database();

    GList *cur;
    qDebug() << "Found " << itdb_tracks_number( d->itdb ) << " tracks in iTunes library";
    for ( cur = d->itdb->tracks; cur; cur = cur->next )
    {
        Itdb_Track *iTrack = ( Itdb_Track * )cur->data;
        if ( !iTrack )
            continue;

        // Compare a track's recent_playcount to the total playcount difference
        // since the last scrobble attempt; scrobbleCount = whichever is
        // smaller.  This is because we cannot guarantee that an iPod's
        // recent_playcount can be reset.  It also lets us Play Well With
        // Others, since other apps like Amarok might also be scrobbling the
        // same device.  We avoid significant over-scrobbles this way.
        int scrobbleCount = qMin( iTrack->recent_playcount,
                                  iTrack->playcount - d->previousPlayCount( iTrack ) ) ;

        if ( scrobbleCount > 0 )
        {
            Track lstTrack;
            d->setTrackInfo( lstTrack, iTrack, scrobbleCount );
            qDebug() << scrobbleCount << "scrobbles found for" << lstTrack;
            d->tracksToScrobble.append( lstTrack );
        }
    }

    d->error = NoError;
    qDebug() << "Procedure took:" << time.elapsed() << "milliseconds";
    emit scrobblingCompleted( d->tracksToScrobble );
}

void
IpodDeviceLinux::finish()
{
    emit finished();
}

void
IpodDeviceLinux::updateDbs()
{
    QTime time;
    time.start();

    if ( d->error != NoError )
        return;

    if ( d->scrobblesDb.isValid() )
    {
        GList *cur;
        d->scrobblesDb.transaction();
        for ( cur = d->itdb->tracks; cur; cur = cur->next )
        {
            Itdb_Track *iTrack = ( Itdb_Track * )cur->data;
            if ( !iTrack )
                continue;

            d->commit( iTrack );
        }
        d->scrobblesDb.commit();
    }

    if ( d->writable )
    {
        GError * err = 0;
        itdb_start_sync( d->itdb );
        bool success = itdb_write( d->itdb, &err );
        if ( !success )
        {
            QString errMsg( "Error writing iTunes database: " );
            if ( err )
            {
                errMsg += err->message;
                g_error_free( err );
            }
            else
            {
                errMsg += "(no error message)";
            }
            qDebug() << errMsg;
        }
        itdb_stop_sync( d->itdb );
    }
    qDebug() << "Updating local and device databases took" << time.elapsed() << "milliseconds.";
}
