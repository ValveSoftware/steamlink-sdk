/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdevicediscovery_udev_p.h"

#include <QStringList>
#include <QCoreApplication>
#include <QObject>
#include <QHash>
#include <QSocketNotifier>
#include <QLoggingCategory>
#include <QDir>
#include <QThread>
#include <QtCore/private/qcore_unix_p.h>

#include <linux/input.h>
#include <fcntl.h>

/* android (and perhaps some other linux-derived stuff) don't define everything
 * in linux/input.h, so we'll need to do that ourselves.
 */
#ifndef KEY_CNT
#define KEY_CNT                 (KEY_MAX+1)
#endif
#ifndef REL_CNT
#define REL_CNT                 (REL_MAX+1)
#endif
#ifndef ABS_CNT
#define ABS_CNT                 (ABS_MAX+1)
#endif

#define LONG_BITS (sizeof(long) * 8 )
#define LONG_FIELD_SIZE(bits) ((bits / LONG_BITS) + 1)

static bool testBit(long bit, const long *field)
{
    return (field[bit / LONG_BITS] >> bit % LONG_BITS) & 1;
}


QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcDD, "qt.qpa.input")

QDeviceDiscovery *QDeviceDiscovery::create(QDeviceTypes types, QObject *parent)
{
    qCDebug(lcDD) << "udev device discovery for type" << types;

    QDeviceDiscovery *helper = 0;
    struct udev *udev;

    udev = udev_new();
    if (udev) {
        helper = new QDeviceDiscoveryUDev(types, udev, parent);
    } else {
        qWarning("Failed to get udev library context");
    }

    return helper;
}

QDeviceDiscoveryUDev::QDeviceDiscoveryUDev(QDeviceTypes types, struct udev *udev, QObject *parent) :
    QDeviceDiscovery(types, parent),
    m_udev(udev), m_udevMonitor(0), m_udevMonitorFileDescriptor(-1), m_udevSocketNotifier(0)
{
    if (!m_udev)
        return;

    m_udevMonitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_udevMonitor) {
        qWarning("Unable to create an udev monitor. No devices can be detected.");
        return;
    }

    udev_monitor_filter_add_match_subsystem_devtype(m_udevMonitor, "input", 0);
    udev_monitor_filter_add_match_subsystem_devtype(m_udevMonitor, "drm", 0);
    udev_monitor_enable_receiving(m_udevMonitor);
    m_udevMonitorFileDescriptor = udev_monitor_get_fd(m_udevMonitor);

    m_udevSocketNotifier = new QSocketNotifier(m_udevMonitorFileDescriptor, QSocketNotifier::Read, this);
    connect(m_udevSocketNotifier, SIGNAL(activated(int)), this, SLOT(handleUDevNotification()));
}

QDeviceDiscoveryUDev::~QDeviceDiscoveryUDev()
{
    if (m_udevMonitor)
        udev_monitor_unref(m_udevMonitor);

    if (m_udev)
        udev_unref(m_udev);
}

QStringList QDeviceDiscoveryUDev::scanConnectedDevices()
{
    QStringList devices;
    QDir dir;
    dir.setFilter(QDir::System);

    // check for input devices
    if (m_types & Device_InputMask) {
        dir.setPath(QString::fromLatin1(QT_EVDEV_DEVICE_PATH));
        foreach (const QString &deviceFile, dir.entryList()) {
            QString absoluteFilePath = dir.absolutePath() + QString::fromLatin1("/") + deviceFile;
            if (checkDeviceType(absoluteFilePath))
                devices << absoluteFilePath;
        }
    }

    // check for drm devices
    if (m_types & Device_VideoMask) {
        dir.setPath(QString::fromLatin1(QT_DRM_DEVICE_PATH));
        foreach (const QString &deviceFile, dir.entryList()) {
            QString absoluteFilePath = dir.absolutePath() + QString::fromLatin1("/") + deviceFile;
            if (checkDeviceType(absoluteFilePath))
                devices << absoluteFilePath;
        }
    }
 
    qCDebug(lcDD) << "Found matching devices" << devices;

    m_devices = devices;

    return devices;
}

void QDeviceDiscoveryUDev::handleUDevNotification()
{
    if (!m_udevMonitor)
        return;

    struct udev_device *dev;
    QString devNode;

    dev = udev_monitor_receive_device(m_udevMonitor);
    if (!dev)
        goto cleanup;

    const char *action;
    action = udev_device_get_action(dev);
    if (!action)
        goto cleanup;

    const char *str;
    str = udev_device_get_devnode(dev);
    if (!str)
        goto cleanup;

    const char *subsystem;
    devNode = QString::fromUtf8(str);
    if (devNode.startsWith(QLatin1String(QT_EVDEV_DEVICE)))
        subsystem = "input";
    else if (devNode.startsWith(QLatin1String(QT_DRM_DEVICE)))
        subsystem = "drm";
    else goto cleanup;

    if (qstrcmp(action, "add") == 0)
    {
        // Wait for the device to finish initialization
        QThread::msleep( 100 );

        if (checkDeviceType(devNode))
        {
            qCDebug(lcDD) << "DeviceDiscovery adding device" << devNode;

            emit deviceDetected(devNode);
            m_devices << devNode;
        }
    }
 
    if (qstrcmp(action, "remove") == 0)
    {
        if (m_devices.contains(devNode))
        {
            qCDebug(lcDD) << "DeviceDiscovery removing device" << devNode;

            emit deviceRemoved(devNode);
            m_devices.removeAll(devNode);
        }
    }

cleanup:
    udev_device_unref(dev);
}

bool QDeviceDiscoveryUDev::checkDeviceType(const QString &device)
{
    bool ret = false;
    int fd = QT_OPEN(device.toLocal8Bit().constData(), O_RDONLY | O_NDELAY, 0);
    if (!fd) {
        qCDebug(lcDD) << "DeviceDiscovery cannot open device" << device;
        return false;
    }

    long bitsKey[LONG_FIELD_SIZE(KEY_CNT)];
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bitsKey)), bitsKey) >= 0 ) {
        if (!ret && (m_types & Device_Keyboard)) {
            if (testBit(KEY_Q, bitsKey)) {
                qCDebug(lcDD) << "DeviceDiscovery found keyboard at" << device;
                ret = true;
            }
        }

        if (!ret && (m_types & Device_Mouse)) {
            long bitsRel[LONG_FIELD_SIZE(REL_CNT)];
            if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(bitsRel)), bitsRel) >= 0 ) {
                if (testBit(REL_X, bitsRel) && testBit(REL_Y, bitsRel) && testBit(BTN_MOUSE, bitsKey)) {
                    qCDebug(lcDD) << "DeviceDiscovery found mouse at" << device;
                    ret = true;
                }
            }
        }

        if (!ret && (m_types & (Device_Touchpad | Device_Touchscreen))) {
            long bitsAbs[LONG_FIELD_SIZE(ABS_CNT)];
            if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(bitsAbs)), bitsAbs) >= 0 ) {
                if (testBit(ABS_X, bitsAbs) && testBit(ABS_Y, bitsAbs)) {
                    if ((m_types & Device_Touchpad) && testBit(BTN_TOOL_FINGER, bitsKey)) {
                        qCDebug(lcDD) << "DeviceDiscovery found touchpad at" << device;
                        ret = true;
                    } else if ((m_types & Device_Touchscreen) && testBit(BTN_TOUCH, bitsKey)) {
                        qCDebug(lcDD) << "DeviceDiscovery found touchscreen at" << device;
                        ret = true;
                    } else if ((m_types & Device_Tablet) && (testBit(BTN_STYLUS, bitsKey) || testBit(BTN_TOOL_PEN, bitsKey))) {
                        qCDebug(lcDD) << "DeviceDiscovery found tablet at" << device;
                        ret = true;
                    }
                }
            }
        }
    }

    if (!ret && (m_types & Device_DRM) && device.contains(QString::fromLatin1(QT_DRM_DEVICE_PREFIX)))
        ret = true;

    QT_CLOSE(fd);
    return ret;
}

QT_END_NAMESPACE
