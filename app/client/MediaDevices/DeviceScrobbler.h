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

#ifndef DEVICE_SCROBBLER_H_
#define DEVICE_SCROBBLER_H_

#include <QDialogButtonBox>
#include <QProcess>

#include "lib/unicorn/UnicornSession.h"
#include "lib/unicorn/UnicornSettings.h"
#include <lastfm/User.h>
#include "IpodDevice.h"

#ifdef Q_WS_X11
#include <QPointer>
#include "IpodDevice_linux.h"
#include "DeviceMonitor.h"
#endif

using unicorn::Session;

class ScrobbleConfirmationDialog;

class DeviceScrobbler : public QObject
{
    Q_OBJECT
public:
    enum DoTwiddlyResult
    {
        Started,
        AlreadyRunning,
        ITunesNotRunning,
        ITunesPluginNotInstalled
    };

public:
    explicit DeviceScrobbler( QObject* parent = 0 );
    ~DeviceScrobbler();

    DoTwiddlyResult doTwiddle( bool manual );

signals:
    void foundScrobbles( const QList<lastfm::Track>& tracks );
    void error( const QString& message );
    void updateDbs();
    void finishIpod();
    void deviceScrobblerFinished();

public slots:
#ifdef Q_WS_X11
    void onScrobbleIpodTriggered();
    void onDeviceMounted( const DeviceMonitor::DeviceInfo& dp );
    void removeDeviceInfo();
#endif

private slots:
#ifdef Q_WS_X11
    void onCalculatingScrobbles();
    void scrobbleIpodTracks( QList<lastfm::Track> );
    void onIpodScrobblingError(IpodDeviceLinux::Error);
    void iterateDevices();
#endif
    void twiddle();
    void onTwiddlyFinished( int, QProcess::ExitStatus );
    void onTwiddlyError( QProcess::ProcessError );

    void onScrobblesConfirmationFinished( int result );
    void checkCachedIPodScrobbles();

public:
    void handleMessage( const QStringList& );
    void iPodDetected( const QStringList& arguments );
#ifdef Q_WS_X11
    bool hasMonitor();
#endif

private:
#ifdef Q_WS_X11
    bool isIpod( QString path );
    void startScrobbleThread( const QString& mountPath, QString serial = "" );
    QString getIpodMountPath();
#endif
    bool isITunesPluginInstalled();

    void twiddled( const QStringList& arguments );
    void scrobbleIpodFiles( const QStringList& files );
    void finalizeScrobbles( QList<lastfm::Track>& tracks );
    QList<lastfm::Track> scrobblesFromFiles( const QStringList& files );

    lastfm::User associatedUser( QString deviceId );

private:
#ifdef Q_WS_X11
    QList<DeviceMonitor::DeviceInfo> m_deviceInfo;
    DeviceMonitor * m_deviceMonitor;
    QPointer<QThread> m_thread;
#endif
    QPointer<QProcess> m_twiddly;
    QTimer* m_twiddlyTimer;
    QPointer<ScrobbleConfirmationDialog> m_confirmDialog;
};

#endif //DEVICE_SCROBBLER_H_
