/****************************************************************************
**
** Copyright (C) 2011-2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2011 Sergey Belyashov <Sergey.Belyashov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
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

#include "qserialportinfo.h"
#include "qserialportinfo_p.h"
#include "qserialport_p.h"

#include <QtCore/qlockfile.h>
#include <QtCore/qfile.h>
#include <QtCore/qdir.h>
#include <QtCore/qscopedpointer.h>

#include <private/qcore_unix_p.h>

#include <errno.h>
#include <sys/types.h> // kill
#include <signal.h>    // kill

#include "qtudev_p.h"

QT_BEGIN_NAMESPACE

static QStringList filteredDeviceFilePaths()
{
    static const QStringList deviceFileNameFilterList = QStringList()

#ifdef Q_OS_LINUX
    << QStringLiteral("ttyS*")    // Standard UART 8250 and etc.
    << QStringLiteral("ttyO*")    // OMAP UART 8250 and etc.
    << QStringLiteral("ttyUSB*")  // Usb/serial converters PL2303 and etc.
    << QStringLiteral("ttyACM*")  // CDC_ACM converters (i.e. Mobile Phones).
    << QStringLiteral("ttyGS*")   // Gadget serial device (i.e. Mobile Phones with gadget serial driver).
    << QStringLiteral("ttyMI*")   // MOXA pci/serial converters.
    << QStringLiteral("ttymxc*")  // Motorola IMX serial ports (i.e. Freescale i.MX).
    << QStringLiteral("ttyAMA*")  // AMBA serial device for embedded platform on ARM (i.e. Raspberry Pi).
    << QStringLiteral("ttyTHS*")  // Serial device for embedded platform on ARM (i.e. Tegra Jetson TK1).
    << QStringLiteral("rfcomm*")  // Bluetooth serial device.
    << QStringLiteral("ircomm*")  // IrDA serial device.
    << QStringLiteral("tnt*");    // Virtual tty0tty serial device.
#elif defined(Q_OS_FREEBSD)
    << QStringLiteral("cu*");
#elif defined(Q_OS_QNX)
    << QStringLiteral("ser*");
#else
    ;
#endif

    QStringList result;

    QDir deviceDir(QStringLiteral("/dev"));
    if (deviceDir.exists()) {
        deviceDir.setNameFilters(deviceFileNameFilterList);
        deviceDir.setFilter(QDir::Files | QDir::System | QDir::NoSymLinks);
        QStringList deviceFilePaths;
        const auto deviceFileInfos = deviceDir.entryInfoList();
        for (const QFileInfo &deviceFileInfo : deviceFileInfos) {
            const QString deviceAbsoluteFilePath = deviceFileInfo.absoluteFilePath();

#ifdef Q_OS_FREEBSD
            // it is a quick workaround to skip the non-serial devices
            if (deviceAbsoluteFilePath.endsWith(QLatin1String(".init"))
                    || deviceAbsoluteFilePath.endsWith(QLatin1String(".lock"))) {
                continue;
            }
#endif

            if (!deviceFilePaths.contains(deviceAbsoluteFilePath)) {
                deviceFilePaths.append(deviceAbsoluteFilePath);
                result.append(deviceAbsoluteFilePath);
            }
        }
    }

    return result;
}

QList<QSerialPortInfo> availablePortsByFiltersOfDevices(bool &ok)
{
    QList<QSerialPortInfo> serialPortInfoList;

    const auto deviceFilePaths = filteredDeviceFilePaths();
    for (const QString &deviceFilePath : deviceFilePaths) {
        QSerialPortInfoPrivate priv;
        priv.device = deviceFilePath;
        priv.portName = QSerialPortInfoPrivate::portNameFromSystemLocation(deviceFilePath);
        serialPortInfoList.append(priv);
    }

    ok = true;
    return serialPortInfoList;
}

static bool isSerial8250Driver(const QString &driverName)
{
    return (driverName == QLatin1String("serial8250"));
}

static bool isValidSerial8250(const QString &systemLocation)
{
#ifdef Q_OS_LINUX
    const mode_t flags = O_RDWR | O_NONBLOCK | O_NOCTTY;
    const int fd = qt_safe_open(systemLocation.toLocal8Bit().constData(), flags);
    if (fd != -1) {
        struct serial_struct serinfo;
        const int retval = ::ioctl(fd, TIOCGSERIAL, &serinfo);
        qt_safe_close(fd);
        if (retval != -1 && serinfo.type != PORT_UNKNOWN)
            return true;
    }
#else
    Q_UNUSED(systemLocation);
#endif
    return false;
}

static bool isRfcommDevice(const QString &portName)
{
    if (!portName.startsWith(QLatin1String("rfcomm")))
        return false;

    bool ok;
    const int portNumber = portName.midRef(6).toInt(&ok);
    if (!ok || (portNumber < 0) || (portNumber > 255))
        return false;
    return true;
}

// provided by the tty0tty driver
static bool isVirtualNullModemDevice(const QString &portName)
{
    return portName.startsWith(QLatin1String("tnt"));
}

static QString ueventProperty(const QDir &targetDir, const QByteArray &pattern)
{
    QFile f(QFileInfo(targetDir, QStringLiteral("uevent")).absoluteFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    const QByteArray content = f.readAll();

    const int firstbound = content.indexOf(pattern);
    if (firstbound == -1)
        return QString();

    const int lastbound = content.indexOf('\n', firstbound);
    return QString::fromLatin1(
                content.mid(firstbound + pattern.size(),
                            lastbound - firstbound - pattern.size()))
            .simplified();
}

static QString deviceName(const QDir &targetDir)
{
    return ueventProperty(targetDir, "DEVNAME=");
}

static QString deviceDriver(const QDir &targetDir)
{
    const QDir deviceDir(targetDir.absolutePath() + QLatin1String("/device"));
    return ueventProperty(deviceDir, "DRIVER=");
}

static QString deviceProperty(const QString &targetFilePath)
{
    QFile f(targetFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
   return QString::fromLatin1(f.readAll()).simplified();
}

static QString deviceDescription(const QDir &targetDir)
{
    return deviceProperty(QFileInfo(targetDir, QStringLiteral("product")).absoluteFilePath());
}

static QString deviceManufacturer(const QDir &targetDir)
{
    return deviceProperty(QFileInfo(targetDir, QStringLiteral("manufacturer")).absoluteFilePath());
}

static quint16 deviceProductIdentifier(const QDir &targetDir, bool &hasIdentifier)
{
    QString result = deviceProperty(QFileInfo(targetDir, QStringLiteral("idProduct")).absoluteFilePath());
    if (result.isEmpty())
        result = deviceProperty(QFileInfo(targetDir, QStringLiteral("device")).absoluteFilePath());
    return result.toInt(&hasIdentifier, 16);
}

static quint16 deviceVendorIdentifier(const QDir &targetDir, bool &hasIdentifier)
{
    QString result = deviceProperty(QFileInfo(targetDir, QStringLiteral("idVendor")).absoluteFilePath());
    if (result.isEmpty())
        result = deviceProperty(QFileInfo(targetDir, QStringLiteral("vendor")).absoluteFilePath());
    return result.toInt(&hasIdentifier, 16);
}

static QString deviceSerialNumber(const QDir &targetDir)
{
    return deviceProperty(QFileInfo(targetDir, QStringLiteral("serial")).absoluteFilePath());
}

QList<QSerialPortInfo> availablePortsBySysfs(bool &ok)
{
    QDir ttySysClassDir(QStringLiteral("/sys/class/tty"));

    if (!(ttySysClassDir.exists() && ttySysClassDir.isReadable())) {
        ok = false;
        return QList<QSerialPortInfo>();
    }

    QList<QSerialPortInfo> serialPortInfoList;
    ttySysClassDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    const auto fileInfos = ttySysClassDir.entryInfoList();
    for (const QFileInfo &fileInfo : fileInfos) {
        if (!fileInfo.isSymLink())
            continue;

        QDir targetDir(fileInfo.symLinkTarget());

        QSerialPortInfoPrivate priv;

        priv.portName = deviceName(targetDir);
        if (priv.portName.isEmpty())
            continue;

        const QString driverName = deviceDriver(targetDir);
        if (driverName.isEmpty()) {
            if (!isRfcommDevice(priv.portName)
                    && !isVirtualNullModemDevice(priv.portName)) {
                continue;
            }
        }

        priv.device = QSerialPortInfoPrivate::portNameToSystemLocation(priv.portName);
        if (isSerial8250Driver(driverName) && !isValidSerial8250(priv.device))
            continue;

        do {
            if (priv.description.isEmpty())
                priv.description = deviceDescription(targetDir);

            if (priv.manufacturer.isEmpty())
                priv.manufacturer = deviceManufacturer(targetDir);

            if (priv.serialNumber.isEmpty())
                priv.serialNumber = deviceSerialNumber(targetDir);

            if (!priv.hasVendorIdentifier)
                priv.vendorIdentifier = deviceVendorIdentifier(targetDir, priv.hasVendorIdentifier);

            if (!priv.hasProductIdentifier)
                priv.productIdentifier = deviceProductIdentifier(targetDir, priv.hasProductIdentifier);

            if (!priv.description.isEmpty()
                    || !priv.manufacturer.isEmpty()
                    || !priv.serialNumber.isEmpty()
                    || priv.hasVendorIdentifier
                    || priv.hasProductIdentifier) {
                break;
            }
        } while (targetDir.cdUp());

        serialPortInfoList.append(priv);
    }

    ok = true;
    return serialPortInfoList;
}

struct ScopedPointerUdevDeleter
{
    static inline void cleanup(struct ::udev *pointer)
    {
        ::udev_unref(pointer);
    }
};

struct ScopedPointerUdevEnumeratorDeleter
{
    static inline void cleanup(struct ::udev_enumerate *pointer)
    {
        ::udev_enumerate_unref(pointer);
    }
};

struct ScopedPointerUdevDeviceDeleter
{
    static inline void cleanup(struct ::udev_device *pointer)
    {
        ::udev_device_unref(pointer);
    }
};

#ifndef LINK_LIBUDEV
    Q_GLOBAL_STATIC(QLibrary, udevLibrary)
#endif

static QString deviceProperty(struct ::udev_device *dev, const char *name)
{
    return QString::fromLatin1(::udev_device_get_property_value(dev, name));
}

static QString deviceDriver(struct ::udev_device *dev)
{
    return QString::fromLatin1(::udev_device_get_driver(dev));
}

static QString deviceDescription(struct ::udev_device *dev)
{
    return deviceProperty(dev, "ID_MODEL").replace(QLatin1Char('_'), QLatin1Char(' '));
}

static QString deviceManufacturer(struct ::udev_device *dev)
{
    return deviceProperty(dev, "ID_VENDOR").replace(QLatin1Char('_'), QLatin1Char(' '));
}

static quint16 deviceProductIdentifier(struct ::udev_device *dev, bool &hasIdentifier)
{
    return deviceProperty(dev, "ID_MODEL_ID").toInt(&hasIdentifier, 16);
}

static quint16 deviceVendorIdentifier(struct ::udev_device *dev, bool &hasIdentifier)
{
    return deviceProperty(dev, "ID_VENDOR_ID").toInt(&hasIdentifier, 16);
}

static QString deviceSerialNumber(struct ::udev_device *dev)
{
    return deviceProperty(dev,"ID_SERIAL_SHORT");
}

static QString deviceName(struct ::udev_device *dev)
{
    return QString::fromLatin1(::udev_device_get_sysname(dev));
}

static QString deviceLocation(struct ::udev_device *dev)
{
    return QString::fromLatin1(::udev_device_get_devnode(dev));
}

QList<QSerialPortInfo> availablePortsByUdev(bool &ok)
{
    ok = false;

#ifndef LINK_LIBUDEV
    static bool symbolsResolved = resolveSymbols(udevLibrary());
    if (!symbolsResolved)
        return QList<QSerialPortInfo>();
#endif

    QScopedPointer<struct ::udev, ScopedPointerUdevDeleter> udev(::udev_new());

    if (!udev)
        return QList<QSerialPortInfo>();

    QScopedPointer<udev_enumerate, ScopedPointerUdevEnumeratorDeleter>
            enumerate(::udev_enumerate_new(udev.data()));

    if (!enumerate)
        return QList<QSerialPortInfo>();

    ::udev_enumerate_add_match_subsystem(enumerate.data(), "tty");
    ::udev_enumerate_scan_devices(enumerate.data());

    udev_list_entry *devices = ::udev_enumerate_get_list_entry(enumerate.data());

    QList<QSerialPortInfo> serialPortInfoList;
    udev_list_entry *dev_list_entry;
    udev_list_entry_foreach(dev_list_entry, devices) {

        ok = true;

        QScopedPointer<udev_device, ScopedPointerUdevDeviceDeleter>
                dev(::udev_device_new_from_syspath(
                        udev.data(), ::udev_list_entry_get_name(dev_list_entry)));

        if (!dev)
            return serialPortInfoList;

        QSerialPortInfoPrivate priv;

        priv.device = deviceLocation(dev.data());
        priv.portName = deviceName(dev.data());

        udev_device *parentdev = ::udev_device_get_parent(dev.data());

        if (parentdev) {
            const QString driverName = deviceDriver(parentdev);
            if (isSerial8250Driver(driverName) && !isValidSerial8250(priv.device))
                continue;
            priv.description = deviceDescription(dev.data());
            priv.manufacturer = deviceManufacturer(dev.data());
            priv.serialNumber = deviceSerialNumber(dev.data());
            priv.vendorIdentifier = deviceVendorIdentifier(dev.data(), priv.hasVendorIdentifier);
            priv.productIdentifier = deviceProductIdentifier(dev.data(), priv.hasProductIdentifier);
        } else {
            if (!isRfcommDevice(priv.portName)
                    && !isVirtualNullModemDevice(priv.portName)) {
                continue;
            }
        }

        serialPortInfoList.append(priv);
    }

    return serialPortInfoList;
}

QList<QSerialPortInfo> QSerialPortInfo::availablePorts()
{
    bool ok;

    QList<QSerialPortInfo> serialPortInfoList = availablePortsByUdev(ok);

#ifdef Q_OS_LINUX
    if (!ok)
        serialPortInfoList = availablePortsBySysfs(ok);
#endif

    if (!ok)
        serialPortInfoList = availablePortsByFiltersOfDevices(ok);

    return serialPortInfoList;
}

#if QT_DEPRECATED_SINCE(5, 6)
bool QSerialPortInfo::isBusy() const
{
    QString lockFilePath = serialPortLockFilePath(portName());
    if (lockFilePath.isEmpty())
        return false;

    QFile reader(lockFilePath);
    if (!reader.open(QIODevice::ReadOnly))
        return false;

    QByteArray pidLine = reader.readLine();
    pidLine.chop(1);
    if (pidLine.isEmpty())
        return false;

    qint64 pid = pidLine.toLongLong();

    if (pid && (::kill(pid, 0) == -1) && (errno == ESRCH))
        return false; // PID doesn't exist anymore

    return true;
}
#endif // QT_DEPRECATED_SINCE(5, 6)

#if QT_DEPRECATED_SINCE(5, 2)
bool QSerialPortInfo::isValid() const
{
    QFile f(systemLocation());
    return f.exists();
}
#endif // QT_DEPRECATED_SINCE(5, 2)

QString QSerialPortInfoPrivate::portNameToSystemLocation(const QString &source)
{
    return (source.startsWith(QLatin1Char('/'))
            || source.startsWith(QLatin1String("./"))
            || source.startsWith(QLatin1String("../")))
            ? source : (QLatin1String("/dev/") + source);
}

QString QSerialPortInfoPrivate::portNameFromSystemLocation(const QString &source)
{
    return source.startsWith(QLatin1String("/dev/"))
            ? source.mid(5) : source;
}

QT_END_NAMESPACE
