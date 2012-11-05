/*
   Copyright 2010-2012 Last.fm Ltd.
      - Primarily authored by Jono Cole and Michael Coffey

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

#include "lib/unicorn/dialogs/ScrobbleConfirmationDialog.h"
#include "lib/unicorn/UnicornApplication.h"
#include "lib/unicorn/QMessageBoxBuilder.h"

#include "../Application.h"
#include "lib/unicorn/widgets/Label.h"
#include "lib/unicorn/dialogs/CloseAppsDialog.h"
#include "DeviceScrobbler.h"
#include "../Services/ScrobbleService/ScrobbleService.h"

#ifdef Q_WS_X11
#include <QFileDialog>
#include <QThread>
#include "HalMonitor.h"
#include "UdisksMonitor.h"
#endif
#include <QDebug>
#include <QDirIterator>
#include <QTimer>

#ifdef Q_OS_MAC
// Check for iTunes playcount difference once every 3 minutes
// (usually takes about 1 sec on Mac)
#define BACKGROUND_CHECK_INTERVAL 3 * 60 * 1000
#else
// On Windows the iPod scrobble check can take around 90 seconds
// for a fairly large library, so only run it every 30 minutes
#define BACKGROUND_CHECK_INTERVAL 30 * 60 * 1000
#endif

DeviceScrobbler::DeviceScrobbler( QObject *parent )
    :QObject( parent )
{
    connect( this, SIGNAL(error(QString)), aApp, SIGNAL(error(QString)));

#ifndef Q_WS_X11
    m_twiddlyTimer = new QTimer( this );
    connect( m_twiddlyTimer, SIGNAL(timeout()), SLOT(twiddle()) );
    m_twiddlyTimer->start( BACKGROUND_CHECK_INTERVAL );

    // run once 3 seconds after starting up
    QTimer::singleShot( 3 * 1000, this, SLOT(twiddle()) );
#else

#ifdef Q_OS_LINUX // udisks is Linux-only
    m_deviceMonitor = new UdisksMonitor( this );
#else
    m_deviceMonitor = new HalMonitor( this );
#endif
    if ( !m_deviceMonitor->isValid() )
    {
        delete m_deviceMonitor;
        m_deviceMonitor = NULL;
    }
    else
    {
        connect( m_deviceMonitor, SIGNAL(deviceMounted(const DeviceMonitor::DeviceInfo&)),
                 this, SLOT(onDeviceMounted(const DeviceMonitor::DeviceInfo&)) );
    }

#endif
}

DeviceScrobbler::~DeviceScrobbler()
{
    if ( m_confirmDialog )
        m_confirmDialog->deleteLater();

    // make sure we terminate twiddly we could be clsoed due
    // to the installer and twiddly would stop install working
    if ( m_twiddly )
        m_twiddly->deleteLater();
}

bool
DeviceScrobbler::isITunesPluginInstalled()
{
#ifdef Q_OS_WIN
    QSettings settings( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Last.fm\\Client\\Plugins\\itw", QSettings::NativeFormat );
    QFile pluginFile( settings.value( "Path" ).toString() );
    return pluginFile.exists();
#else
    return true;
#endif
}

void
DeviceScrobbler::twiddle()
{
    doTwiddle( false );
}

DeviceScrobbler::DoTwiddlyResult
DeviceScrobbler::doTwiddle( bool manual )
{
#ifndef Q_WS_X11
    if ( unicorn::CloseAppsDialog::isITunesRunning() )
    {
        if ( isITunesPluginInstalled() )
        {
            if ( m_twiddly )
            {
                qWarning() << "m_twiddly already running. Early out.";
                return AlreadyRunning;
            }

            //"--device diagnostic --vid 0000 --pid 0000 --serial UNKNOWN

            QStringList args = (QStringList()
                         << "--device" << "background"
                         << "--vid" << "0000"
                         << "--pid" << "0000"
                         << "--serial" << "UNKNOWN");

            if ( manual )
                args += "--manual";

            m_twiddly = new QProcess( this );
            connect( m_twiddly, SIGNAL(finished( int, QProcess::ExitStatus )), SLOT(onTwiddlyFinished( int, QProcess::ExitStatus )) );
            connect( m_twiddly, SIGNAL(error( QProcess::ProcessError )), SLOT(onTwiddlyError( QProcess::ProcessError )) );
#ifdef Q_OS_WIN
            m_twiddly->start( QDir( QCoreApplication::applicationDirPath() ).absoluteFilePath( "iPodScrobbler.exe" ), args );
#else
            m_twiddly->start( QDir( QCoreApplication::applicationDirPath() ).absoluteFilePath( "../Helpers/iPodScrobbler" ), args );
#endif
            return Started;
        }
        else
            return ITunesPluginNotInstalled;
    }
#endif //  Q_WS_X11
    return ITunesNotRunning;
}

void
DeviceScrobbler::onTwiddlyFinished( int exitCode, QProcess::ExitStatus exitStatus )
{
    qDebug() << exitCode << exitStatus;
    m_twiddly->deleteLater();
}

void
DeviceScrobbler::onTwiddlyError( QProcess::ProcessError error )
{
    qDebug() << error;
    m_twiddly->deleteLater();
}

void 
DeviceScrobbler::checkCachedIPodScrobbles()
{
    // This is a bit awkward, since there's no ipod scrobble cache for
    // Q_WS_X11.  Instead, Q_WS_X11 uses this to check for plugged in devices
    // on startup, then to set up connections to the mount watcher.
    // Why use this at all for Q_WS_X11?  We want to take advantage of that 3
    // second delay in ScrobbleService::resetScrobbler()

#ifndef Q_WS_X11

    QStringList files;

    // check if there are any iPod scrobbles in its folder
    QDir scrobblesDir = lastfm::dir::runtimeData();

    if ( scrobblesDir.cd( "devices" ) )
    {
        QDirIterator iterator( scrobblesDir, QDirIterator::Subdirectories );

        while ( iterator.hasNext() )
        {
            iterator.next();

            if ( iterator.fileInfo().isFile() )
            {
                QString filename = iterator.fileName();

                if ( filename.endsWith(".xml") )
                    files << iterator.fileInfo().absoluteFilePath();
            }
        }
    }

    scrobbleIpodFiles( files );

#else
    if ( m_deviceMonitor )
    {
        onScrobbleIpodTriggered();
        m_deviceMonitor->watchDeviceChanges();
    }
#endif
}



void 
DeviceScrobbler::handleMessage( const QStringList& message )
{
    int pos = message.indexOf( "--twiddly" );
    const QString& action = message[ pos + 1 ];
    
    if( action == "complete" )
        twiddled( message );
    else if ( action == "incompatible-plugin" )
        emit error( tr( "Device scrobbling disabled - incompatible iTunes plugin - %1" )
                    .arg( unicorn::Label::anchor( "plugin", tr("please update" ) ) ) );
}


void 
DeviceScrobbler::iPodDetected( const QStringList& /*arguments*/ )
{
}

void 
DeviceScrobbler::twiddled( const QStringList& arguments )
{
    // iPod scrobble time!
    QString iPodPath = arguments[ arguments.indexOf( "--ipod-path" ) + 1 ];

    if ( !arguments.contains( "no-tracks-found" ) )
        scrobbleIpodFiles( QStringList( iPodPath ) );
}



void 
DeviceScrobbler::scrobbleIpodFiles( const QStringList& files )
{
    qDebug() << files;

    bool removeFiles = false;

    if ( unicorn::OldeAppSettings().deviceScrobblingEnabled() )
    {
        QList<lastfm::Track> scrobbles = scrobblesFromFiles( files );

        // TODO: fix the root cause of this problem
        // If there are more than 4000 scrobbles we assume there was an error with the
        // iPod scrobbling diff checker so discard these scrobbles.
        // 4000 because 16 waking hours a day, for two weeks, with 3.5 minute songs
        if ( scrobbles.count() >= 4000 )
            removeFiles = true;
        else
        {
            if ( scrobbles.count() > 0 )
            {
                if ( unicorn::AppSettings().alwaysAsk()
                     || scrobbles.count() >= 200 ) // always get them to check scrobbles over 200
                {
                    if ( !m_confirmDialog )
                    {
                        m_confirmDialog = new ScrobbleConfirmationDialog( scrobbles, aApp->mainWindow() );
                        connect( m_confirmDialog, SIGNAL(finished(int)), SLOT(onScrobblesConfirmationFinished(int)) );
                    }
                    else
                        m_confirmDialog->addTracks( scrobbles );

                    // add the files so it can delete them when the user has decided what to do
                    m_confirmDialog->addFiles( files );
                    m_confirmDialog->show();
                }
                else
                {
                    // sort the iPod scrobbles before caching them
                    if ( scrobbles.count() > 1 )
                        qSort ( scrobbles.begin(), scrobbles.end() );

                    emit foundScrobbles( scrobbles );

                    // we're scrobbling them so remove the source files
                    removeFiles = true;
                }
            }
            else
                // there were no scrobbles in the files so remove them
                removeFiles = true;
        }
    }
    else
        // device scrobbling is disabled so remove these files
        removeFiles = true;

    if ( removeFiles )
        foreach ( QString file, files )
            QFile::remove( file );

}

QList<lastfm::Track>
DeviceScrobbler::scrobblesFromFiles( const QStringList& files  )
{
    QList<lastfm::Track> scrobbles;

    foreach ( const QString file, files )
    {
        QFile iPodScrobbleFile( file );

        if ( iPodScrobbleFile.open( QIODevice::ReadOnly | QIODevice::Text ) )
        {
            QDomDocument iPodScrobblesDoc;
            iPodScrobblesDoc.setContent( &iPodScrobbleFile );
            QDomNodeList tracks = iPodScrobblesDoc.elementsByTagName( "track" );

            for ( int i(0) ; i < tracks.count() ; ++i )
            {
                lastfm::Track track( tracks.at(i).toElement() );

                // don't add tracks to the list if they don't have an artist
                // don't add podcasts to the list if podcast scrobbling is off
                // don't add videos to the list (well, videos that aren't "music video")
                // don't add tracks if they are in excluded folders

                if ( !track.artist().isNull()
                     && ( unicorn::UserSettings().value( "podcasts", true ).toBool() || !track.isPodcast() )
                     && !track.isVideo()
                     && !ScrobbleService::isDirExcluded( track ) )
                    scrobbles << track;
            }
        }
    }

    return scrobbles;
}

void
DeviceScrobbler::onScrobblesConfirmationFinished( int result )
{
    if ( result == QDialog::Accepted )
    {
        QList<lastfm::Track> scrobbles = m_confirmDialog->tracksToScrobble();

        // sort the iPod scrobbles before caching them
        if ( scrobbles.count() > 1 )
            qSort ( scrobbles.begin(), scrobbles.end() );

        emit foundScrobbles( scrobbles );

        unicorn::AppSettings().setAlwaysAsk( !m_confirmDialog->autoScrobble() );
    }

    // delete all the iPod scrobble files whether it was accepted or not
    foreach ( const QString file, m_confirmDialog->files() )
        QFile::remove( file );

    m_confirmDialog->deleteLater();
}

#ifdef Q_WS_X11
bool
DeviceScrobbler::hasMonitor()
{
    if ( m_deviceMonitor )
        return true;
    return false;
}

void 
DeviceScrobbler::onScrobbleIpodTriggered() {
    if ( m_deviceMonitor )
    {
        m_deviceMonitor->startupScan();
    }
    else
    {
        QString mp = getIpodMountPath();
        if ( isIpod( mp ) )
            startScrobbleThread( mp );
        else
            QMessageBoxBuilder( 0 )
                .setIcon( QMessageBox::Critical )
                .setTitle( tr( "Scrobble iPod" ) )
                .setText( tr( "The mount path does not appear to be an iPod." ) )
                .exec();
    }
}

void
DeviceScrobbler::iterateDevices()
{
    // One at a time, please
    if ( m_thread && m_thread->isRunning() )
        return;

    if ( m_deviceInfo.count() )
    {
        DeviceMonitor::DeviceInfo info = m_deviceInfo.first();
        if ( isIpod( info.mountPath ) )
        {
            startScrobbleThread( info.mountPath, info.deviceId );
        }
        else
        {
            iterateDevices();
        }
    }
    else
    {
        emit deviceScrobblerFinished();
    }
}

void
DeviceScrobbler::onDeviceMounted( const DeviceMonitor::DeviceInfo& info )
{
    QList<DeviceMonitor::DeviceInfo>::const_iterator i;
    for ( i = m_deviceInfo.constBegin(); i != m_deviceInfo.constEnd(); ++i )
    {
        if ( info.mountPath == (*i).mountPath )
            return;
    }
    m_deviceInfo.append( info );
    iterateDevices();
}

void
DeviceScrobbler::removeDeviceInfo()
{
    if ( m_deviceInfo.count() )
        m_deviceInfo.removeFirst();
}

bool
DeviceScrobbler::isIpod( QString path )
{
    if ( path.isEmpty() )
        return false;
    // Basic test to see if this is an iPod
    QFileInfo fileInfo( path );
    if ( fileInfo.isDir() &&
         ( QFile::exists( path + "/iTunes_Control") ||
           QFile::exists( path + "/iPod_Control") ||
           QFile::exists( path + "/iTunes/iTunes_Control")
         )
       )
    {
        return true;
    }
    else
        return false;
}

void
DeviceScrobbler::startScrobbleThread( const QString& mountPath, QString serial )
{
    m_thread = new QThread( this );
    IpodDeviceLinux * iPod = new IpodDeviceLinux( mountPath, serial );
    iPod->moveToThread(m_thread);
    qRegisterMetaType< QList<lastfm::Track> >("QList<lastfm::Track>");
    qRegisterMetaType< IpodDeviceLinux::Error >("IpodDeviceLinux::Error");

    connect( iPod, SIGNAL( calculatingScrobbles() ), this, SLOT( onCalculatingScrobbles() ) );
    connect( iPod, SIGNAL( scrobblingCompleted( QList<lastfm::Track> ) ), this, SLOT( scrobbleIpodTracks( QList<lastfm::Track> ) ) );
    connect( iPod, SIGNAL( errorOccurred(IpodDeviceLinux::Error) ), this, SLOT( onIpodScrobblingError(IpodDeviceLinux::Error) ) );

    connect(m_thread, SIGNAL(started()), iPod, SLOT( fetchTracksToScrobble()) );
    connect(iPod, SIGNAL(finished()), iPod, SLOT(deleteLater()) );
    connect(iPod, SIGNAL(finished()), this, SLOT(removeDeviceInfo()) );
    connect(iPod, SIGNAL(destroyed(QObject*)), m_thread, SLOT(quit()) );
    connect(m_thread, SIGNAL(finished()), m_thread, SLOT(deleteLater()) );
    connect(m_thread, SIGNAL(destroyed(QObject*)), this, SLOT(iterateDevices()) );
    connect(this, SIGNAL(finishIpod()), iPod, SLOT(finish()) );
    connect(this, SIGNAL(updateDbs()), iPod, SLOT(updateDbs()) );
    m_thread->start();
}

QString
DeviceScrobbler::getIpodMountPath()
{
    QString path = "";
    QFileDialog dialog( 0, QObject::tr( "Where is your iPod mounted?" ), "/" );
    dialog.setOption( QFileDialog::ShowDirsOnly, true );
    dialog.setFileMode( QFileDialog::Directory );

    //The following lines are to make sure the QFileDialog looks native.
    QString backgroundColor( "transparent" );
    dialog.setStyleSheet( "QDockWidget QFrame{ background-color: " + backgroundColor + "; }" );

    if ( dialog.exec() )
    {
       path = dialog.selectedFiles()[ 0 ];
    }
    return path;
}

void 
DeviceScrobbler::onCalculatingScrobbles()
{
    qApp->setOverrideCursor( Qt::WaitCursor );
}

void
DeviceScrobbler::finalizeScrobbles( QList<lastfm::Track>& tracks )
{
    // sort the iPod scrobbles before caching them
    if ( tracks.count() > 1 )
        qSort ( tracks.begin(), tracks.end() );

    emit updateDbs();
    emit foundScrobbles( tracks );
}

void
DeviceScrobbler::scrobbleIpodTracks( QList<lastfm::Track> tracks )
{

    qApp->restoreOverrideCursor();
    qDebug() << tracks.count() << "new tracks to scrobble.";

    if ( tracks.count() )
    {
        if( unicorn::AppSettings().alwaysAsk() )
        {
            ScrobbleConfirmationDialog confirmDialog( tracks );
            if ( confirmDialog.exec() == QDialog::Accepted )
            {
                tracks = confirmDialog.tracksToScrobble();
                finalizeScrobbles( tracks );
            }
        }
        else
        {
            finalizeScrobbles( tracks );
        }
    }
    else
    {
        QMessageBoxBuilder( 0 )
            .setIcon( QMessageBox::Information )
            .setTitle( tr( "Scrobble iPod" ) )
            .setText( tr( "No tracks to scrobble since your last sync." ) )
            .exec();
    }
    emit finishIpod();
}

void 
DeviceScrobbler::onIpodScrobblingError(IpodDeviceLinux::Error error)
{
    qDebug() << "iPod Error";
    qApp->restoreOverrideCursor();
    QString path;
    switch( error )
    {
        case IpodDeviceLinux::AccessError:
            QMessageBoxBuilder( 0 )
                .setIcon( QMessageBox::Critical )
                .setTitle( tr( "Scrobble iPod" ) )
                .setText( tr( "The iPod database could not be opened." ) )
                .exec();
            break;
        case IpodDeviceLinux::UnknownError:
            QMessageBoxBuilder( 0 )
                .setIcon( QMessageBox::Critical )
                .setTitle( tr( "Scrobble iPod" ) )
                .setText( tr( "An unknown error occurred while trying to access the iPod database." ) )
                .exec();
            break;
        default:
            qDebug() << "untracked error";
    }
    emit finishIpod();
}

#endif


