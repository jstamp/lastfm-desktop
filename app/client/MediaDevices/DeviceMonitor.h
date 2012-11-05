/*
 * DeviceMonitor: basic D-Bus interface to watch for volume mount signals
 * Copyright (C) 2012 John Stamp <jstamp@mehercule.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEVICEMONITOR_H_
#define DEVICEMONITOR_H_

#include <QObject>
#include <QString>

class DeviceMonitor : public QObject
{
Q_OBJECT
public:
    DeviceMonitor( QObject * parent ) : QObject( parent ) {};
    virtual void watchDeviceChanges() = 0;
    virtual bool isValid() const = 0;
    virtual void startupScan() = 0;

    struct DeviceInfo
    {
        QString mountPath;
        QString deviceId;
    };

signals:
    void deviceMounted( const DeviceMonitor::DeviceInfo& );
};

#endif
