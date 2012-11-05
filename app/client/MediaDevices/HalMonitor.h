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

#ifndef HALMONITOR_H_
#define HALMONITOR_H_

#include "DeviceMonitor.h"

#include <QHash>

class QDBusInterface;
class HalDevice;

class HalMonitor : public DeviceMonitor
{
Q_OBJECT
public:
    HalMonitor( QObject * parent );
    ~HalMonitor();
    void watchDeviceChanges();
    bool isValid() const;
    void startupScan();

private slots:
    void onDeviceAdded( const QString& udi );
    void onDeviceRemoved( const QString& udi );
    void onVolumeMounted( const DeviceMonitor::DeviceInfo& dp );

private:
    void clearDevices();
    QDBusInterface * m_dbusInterface;
    QHash<QString, HalDevice*> m_devHash;
};

#endif
