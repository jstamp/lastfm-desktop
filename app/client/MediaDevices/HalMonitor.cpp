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

#include "HalMonitor.h"

#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>

#define HAL_SERVICE   "org.freedesktop.Hal"
#define HAL_PATH      "/org/freedesktop/Hal/Manager"
#define HAL_INTERFACE "org.freedesktop.Hal.Manager"
#define HAL_DEVICE    "org.freedesktop.Hal.Device"

struct ChangeDescription
{
    QString key;
    bool added;
    bool removed;
};

Q_DECLARE_METATYPE(ChangeDescription)
Q_DECLARE_METATYPE(QList<ChangeDescription>)

const QDBusArgument &operator<<( QDBusArgument &arg, const ChangeDescription &change )
{
    arg.beginStructure();
    arg << change.key << change.added << change.removed;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>( const QDBusArgument &arg, ChangeDescription &change )
{
    arg.beginStructure();
    arg >> change.key >> change.added >> change.removed;
    arg.endStructure();
    return arg;
}

class HalDevice : public QObject
{
Q_OBJECT
public:
    HalDevice( const QString& udi, QObject * parent = 0 );
    ~HalDevice();
    bool isRemovableVolume() const;
    bool isMounted() const;
    QString mountPath() const;
    QString serial() const;

signals:
    void volumeMounted( const DeviceMonitor::DeviceInfo& info );

private slots:
    void slotPropertyModified( int, const QList<ChangeDescription>& );

private:
    QDBusInterface * m_dbusInterface;
    QDBusInterface * m_devInterface;
    QString m_mountPath;
    QString m_serial;
    bool m_isMounted;
    bool m_removableVol;
};

HalDevice::HalDevice( const QString& udi, QObject * parent )
    : QObject( parent )
    , m_isMounted( false )
    , m_removableVol( false )
{
    m_dbusInterface = new QDBusInterface( HAL_SERVICE,
                                          udi,
                                          HAL_DEVICE,
                                          QDBusConnection::systemBus(),
                                          this
                                        );

    if ( m_dbusInterface->call( "GetProperty", "info.category" ).arguments().at(0).toString() == "volume" &&
         !m_dbusInterface->call( "GetProperty", "volume.is_partition" ).arguments().at(0).toBool() &&
         !m_dbusInterface->call( "GetProperty", "volume.ignore" ).arguments().at(0).toBool() )
    {
        QDBusReply<QString> devReply = m_dbusInterface->call( "GetProperty", "block.storage_device" );
        m_devInterface = new QDBusInterface ( HAL_SERVICE,
                                              devReply.value(),
                                              HAL_DEVICE,
                                              QDBusConnection::systemBus(),
                                              this
                                            );
        if ( m_devInterface->call( "GetProperty", "storage.removable" ).arguments().at(0).toBool() ||
             m_devInterface->call( "GetProperty", "storage.hotpluggable" ).arguments().at(0).toBool() )
        {
            m_removableVol = true;
            m_isMounted = m_dbusInterface->call( "GetProperty", "volume.is_mounted" ).arguments().at(0).toBool();
            m_mountPath = m_dbusInterface->call( "GetProperty", "volume.mount_point" ).arguments().at(0).toString();
            m_serial = m_devInterface->call( "GetProperty", "storage.serial" ).arguments().at(0).toString();
            m_dbusInterface->connection().connect( HAL_SERVICE,
                                                   udi,
                                                   HAL_DEVICE,
                                                   "PropertyModified",
                                                   this,
                                                   SLOT(slotPropertyModified(int, const QList<ChangeDescription>&))
                                                  );
        }
    }
}

HalDevice::~HalDevice()
{
}

bool
HalDevice::isMounted() const
{
    return m_isMounted;
}

QString
HalDevice::mountPath() const
{
    return m_mountPath;
}

QString
HalDevice::serial() const
{
    return m_serial;
}

bool
HalDevice::isRemovableVolume() const
{
    return m_removableVol;
}

void
HalDevice::slotPropertyModified( int, const QList<ChangeDescription>& properties )
{
    QList<ChangeDescription>::const_iterator it = properties.begin();
    while ( it != properties.end() )
    {
        if ( (*it).key == "volume.is_mounted" )
        {
            m_isMounted = m_dbusInterface->call( "GetProperty", "volume.is_mounted" ).arguments().at(0).toBool();
            m_mountPath = m_dbusInterface->call( "GetProperty", "volume.mount_point" ).arguments().at(0).toString();
            if ( m_isMounted && !m_mountPath.isEmpty() )
            {
                DeviceMonitor::DeviceInfo info;
                info.mountPath = m_mountPath;
                info.deviceId = m_serial;
                emit volumeMounted( info );
            }
            break;
        }
        ++it;
    }
}


HalMonitor::HalMonitor( QObject * parent )
    : DeviceMonitor( parent )
{
    qDBusRegisterMetaType<ChangeDescription>();
    qDBusRegisterMetaType< QList<ChangeDescription> >();
    m_dbusInterface = new QDBusInterface( HAL_SERVICE,
                                          HAL_PATH,
                                          HAL_INTERFACE,
                                          QDBusConnection::systemBus(),
                                          this );
}

HalMonitor::~HalMonitor()
{
    clearDevices();
}

void
HalMonitor::watchDeviceChanges()
{
    m_dbusInterface->connection().connect( HAL_SERVICE,
                                           HAL_PATH,
                                           HAL_INTERFACE,
                                           "DeviceAdded",
                                           this,
                                           SLOT(onDeviceAdded(const QString&))
                                         );
    m_dbusInterface->connection().connect( HAL_SERVICE,
                                           HAL_PATH,
                                           HAL_INTERFACE,
                                           "DeviceRemoved",
                                           this,
                                           SLOT(onDeviceRemoved(const QString&))
                                         );
}

void
HalMonitor::onDeviceRemoved( const QString& udi )
{
    QHash<QString, HalDevice*>::iterator it = m_devHash.begin();
    while ( it != m_devHash.end() )
    {
        if ( it.key() == udi )
        {
            HalDevice * dev = it.value();
            it = m_devHash.erase( it );
            dev->deleteLater();
        }
        else
        {
            ++it;
        }
    }
}

void
HalMonitor::onDeviceAdded( const QString& udi )
{
    if ( m_devHash.keys().contains( udi ) )
        return;
    HalDevice * vol = new HalDevice( udi, this );
    if ( vol->isRemovableVolume() )
    {
        m_devHash.insert( udi, vol );
        if ( vol->isMounted() )
        {
            DeviceMonitor::DeviceInfo info;
            info.mountPath = vol->mountPath();
            info.deviceId = vol->serial();
            emit deviceMounted( info );
        }
        connect( vol, SIGNAL(volumeMounted(const DeviceMonitor::DeviceInfo&)),
                 this, SLOT(onVolumeMounted(const DeviceMonitor::DeviceInfo&)) );
    }
    else
    {
        delete vol;
    }
}

void
HalMonitor::onVolumeMounted( const DeviceMonitor::DeviceInfo& info )
{
    emit deviceMounted( info );
}

void
HalMonitor::clearDevices()
{
    QHash<QString, HalDevice*>::iterator it = m_devHash.begin();
    while ( it != m_devHash.end() )
    {
        HalDevice * dev = it.value();
        it = m_devHash.erase( it );
        dev->deleteLater();
    }
}

void
HalMonitor::startupScan()
{
    // First clear out anything that's already there
    clearDevices();

    QDBusReply <QStringList> reply = m_dbusInterface->call( "FindDeviceByCapability", "volume" );
    if (reply.isValid())
    {
        foreach ( QString udi, reply.value() )
        {
            onDeviceAdded( udi );
        }
    }
}

bool
HalMonitor::isValid() const
{
    return m_dbusInterface->isValid();
}

#include "HalMonitor.moc"
