/*
   Copyright 2005-2010 Last.fm Ltd.
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
#ifndef IPOD_DEVICE_LINUX_H
#define IPOD_DEVICE_LINUX_H

#include "MediaDevice.h"

class IpodDeviceLinux: public MediaDevice
{
    Q_OBJECT

public:

    IpodDeviceLinux( const QString& mountPath, const QString& deviceSerial = "" );
    ~IpodDeviceLinux();

signals:
    void finished();

public slots:
    /**
     * Fetches the tracks from the iPod.
     */
    void fetchTracksToScrobble();

    /**
     * reset the recent playcounts.
     * used after scrobbles have been found and optionally approved
     */
    void updateDbs();
    void finish();

private:
    class IpodDeviceLinuxPrivate* d;
};

#endif // IPOD_DEVICE_H
