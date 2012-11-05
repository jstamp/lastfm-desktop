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

#ifndef UDISKSMONITOR_H_
#define UDISKSMONITOR_H_

#include "DeviceMonitor.h"

class QDBusInterface;
class QDBusObjectPath;

class UdisksMonitor : public DeviceMonitor
{
Q_OBJECT
public:
    UdisksMonitor( QObject * parent );
    void watchDeviceChanges();
    bool isValid() const;
    void startupScan();

private slots:
    void onDeviceChanged( const QDBusObjectPath& );

private:
    QDBusInterface * m_dbusInterface;
};

#endif
