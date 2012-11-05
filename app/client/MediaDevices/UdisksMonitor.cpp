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

#include "UdisksMonitor.h"

#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>

#define UDISKS_SERVICE   "org.freedesktop.UDisks"
#define UDISKS_PATH      "/org/freedesktop/UDisks"
#define UDISKS_INTERFACE "org.freedesktop.UDisks"
#define UDISKS_DEVICE    "org.freedesktop.UDisks.Device"


UdisksMonitor::UdisksMonitor( QObject * parent )
    : DeviceMonitor( parent )
{
    m_dbusInterface = new QDBusInterface( UDISKS_SERVICE,
                                          UDISKS_PATH,
                                          UDISKS_INTERFACE,
                                          QDBusConnection::systemBus(),
                                          this );

    if ( !m_dbusInterface->isValid() )
    {
        delete m_dbusInterface;
        m_dbusInterface = NULL;
        QDBusMessage message = QDBusMessage::createMethodCall( "org.freedesktop.DBus",
                                                               "/org/freedesktop/DBus",
                                                               "org.freedesktop.DBus",
                                                               "ListActivatableNames"
                                                             );

        QDBusReply<QStringList> reply = QDBusConnection::systemBus().call( message );
        if ( reply.isValid() && reply.value().contains( UDISKS_SERVICE ) )
        {
            QDBusConnection::systemBus().interface()->startService( UDISKS_SERVICE );
            m_dbusInterface = new QDBusInterface( UDISKS_SERVICE,
                                                  UDISKS_PATH,
                                                  UDISKS_INTERFACE,
                                                  QDBusConnection::systemBus(),
                                                  this );
        }
    }
}

void
UdisksMonitor::watchDeviceChanges()
{
    QDBusConnection::systemBus().connect( UDISKS_SERVICE,
                                          UDISKS_PATH,
                                          UDISKS_INTERFACE,
                                          "DeviceChanged",
                                          this,
                                          SLOT( onDeviceChanged( const QDBusObjectPath& ) )
                                        );
}

void
UdisksMonitor::onDeviceChanged( const QDBusObjectPath& objPath )
{
    QDBusInterface devInterface( UDISKS_SERVICE,
                                 objPath.path(),
                                 UDISKS_DEVICE,
                                 QDBusConnection::systemBus(),
                                 this
                               );

    // Skip internal, hidden, unmounted drives & parition tables
    if ( !devInterface.isValid() ||
         devInterface.property( "DeviceIsSystemInternal" ).toBool() ||
         devInterface.property( "DevicePresentationHide" ).toBool() ||
         devInterface.property( "DeviceMountPaths" ).toStringList().isEmpty() ||
         devInterface.property( "DeviceIsPartitionTable" ).toBool()
       )
    {
        return;
    }

    DeviceInfo info;
    info.deviceId = devInterface.property( "DriveSerial" ).toString();
    info.mountPath = devInterface.property( "DeviceMountPaths" ).toStringList().first();
    emit deviceMounted( info );
}

void
UdisksMonitor::startupScan()
{
    if ( !m_dbusInterface )
        return;

    QDBusReply <QList<QDBusObjectPath> > reply = m_dbusInterface->call( "EnumerateDevices" );
    if ( reply.isValid() )
    {
        foreach( QDBusObjectPath i, reply.value() )
        {
            onDeviceChanged( i );
        }
    }
}

bool
UdisksMonitor::isValid() const
{
    if ( m_dbusInterface )
        return m_dbusInterface->isValid();
    else
        return false;
}
