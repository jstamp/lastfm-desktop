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
#ifndef MEDIA_DEVICE_H
#define MEDIA_DEVICE_H

#include <QObject>
#include <QSqlDatabase>
#include <lastfm/Track.h>
#include <lastfm/User.h>

#define DB_NAME "MediaDevicesScrobbles"

class MediaDevice: public QObject
{
    Q_OBJECT

public:
    enum Error
    {
        NoError,
        AccessError,
        UnknownError
    };

    MediaDevice();

signals:
    void deviceScrobblingStarted();
    void calculatingScrobbles();
    void scrobblingCompleted( QList<lastfm::Track> );
    void errorOccurred(MediaDevice::Error);
};

#endif // MEDIA_DEVICE_H
