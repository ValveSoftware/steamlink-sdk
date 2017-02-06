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

#include "private/qcore_mac_p.h"

#include <sys/param.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h> // for kIOPropertyProductNameKey
#include <IOKit/usb/USB.h>
#if defined(MAC_OS_X_VERSION_10_4) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4)
#  include <IOKit/serial/ioss.h>
#endif
#include <IOKit/IOBSD.h>

QT_BEGIN_NAMESPACE

static QCFType<CFTypeRef> searchProperty(io_registry_entry_t ioRegistryEntry,
                                         const QCFString &propertyKey)
{
    return ::IORegistryEntrySearchCFProperty(
                ioRegistryEntry, kIOServicePlane, propertyKey, kCFAllocatorDefault, 0);
}

static QString searchStringProperty(io_registry_entry_t ioRegistryEntry,
                                    const QCFString &propertyKey)
{
    const QCFType<CFTypeRef> result(searchProperty(ioRegistryEntry, propertyKey));
    const CFStringRef ref = result.as<CFStringRef>();
    if (ref && (::CFGetTypeID(ref) == ::CFStringGetTypeID()))
        return QString::fromCFString(ref);
    return QString();
}

static quint16 searchShortIntProperty(io_registry_entry_t ioRegistryEntry,
                                      const QCFString &propertyKey,
                                      bool &ok)
{
    const QCFType<CFTypeRef> result(searchProperty(ioRegistryEntry, propertyKey));
    const CFNumberRef ref = result.as<CFNumberRef>();
    quint16 value = 0;
    ok = ref && (::CFGetTypeID(ref) == ::CFNumberGetTypeID())
            && (::CFNumberGetValue(ref, kCFNumberShortType, &value) > 0);
    return value;
}

static bool isCompleteInfo(const QSerialPortInfoPrivate &priv, const QString &calloutDevice, const QString &dialinDevice)
{
    return !calloutDevice.isEmpty()
            && !dialinDevice.isEmpty()
            && !priv.manufacturer.isEmpty()
            && !priv.description.isEmpty()
            && !priv.serialNumber.isEmpty()
            && priv.hasProductIdentifier
            && priv.hasVendorIdentifier;
}

static QString calloutDeviceSystemLocation(io_registry_entry_t ioRegistryEntry)
{
    return searchStringProperty(ioRegistryEntry, QCFString(CFSTR(kIOCalloutDeviceKey)));
}

static QString dialinDeviceSystemLocation(io_registry_entry_t ioRegistryEntry)
{
    return searchStringProperty(ioRegistryEntry, QCFString(CFSTR(kIODialinDeviceKey)));
}

static QString deviceDescription(io_registry_entry_t ioRegistryEntry)
{
    QString result = searchStringProperty(ioRegistryEntry, QCFString(CFSTR(kIOPropertyProductNameKey)));
    if (result.isEmpty())
        result = searchStringProperty(ioRegistryEntry, QCFString(CFSTR(kUSBProductString)));
    if (result.isEmpty())
        result = searchStringProperty(ioRegistryEntry, QCFString(CFSTR("BTName")));
    return result;
}

static QString deviceManufacturer(io_registry_entry_t ioRegistryEntry)
{
    return searchStringProperty(ioRegistryEntry, QCFString(CFSTR(kUSBVendorString)));
}

static QString deviceSerialNumber(io_registry_entry_t ioRegistryEntry)
{
    return searchStringProperty(ioRegistryEntry, QCFString(CFSTR(kUSBSerialNumberString)));
}

static quint16 deviceVendorIdentifier(io_registry_entry_t ioRegistryEntry, bool &ok)
{
    return searchShortIntProperty(ioRegistryEntry, QCFString(CFSTR(kUSBVendorID)), ok);
}

static quint16 deviceProductIdentifier(io_registry_entry_t ioRegistryEntry, bool &ok)
{
    return searchShortIntProperty(ioRegistryEntry, QCFString(CFSTR(kUSBProductID)), ok);
}

static io_registry_entry_t parentSerialPortService(io_registry_entry_t currentSerialPortService)
{
    io_registry_entry_t result = 0;
    ::IORegistryEntryGetParentEntry(currentSerialPortService, kIOServicePlane, &result);
    ::IOObjectRelease(currentSerialPortService);
    return result;
}

QList<QSerialPortInfo> QSerialPortInfo::availablePorts()
{
    CFMutableDictionaryRef serialPortDictionary = ::IOServiceMatching(kIOSerialBSDServiceValue);
    if (!serialPortDictionary)
        return QList<QSerialPortInfo>();

    ::CFDictionaryAddValue(serialPortDictionary,
                           CFSTR(kIOSerialBSDTypeKey),
                           CFSTR(kIOSerialBSDAllTypes));

    io_iterator_t serialPortIterator = 0;
    if (::IOServiceGetMatchingServices(kIOMasterPortDefault, serialPortDictionary,
                                       &serialPortIterator) != KERN_SUCCESS) {
        return QList<QSerialPortInfo>();
    }

    QList<QSerialPortInfo> serialPortInfoList;

    for (;;) {
        io_registry_entry_t serialPortService = ::IOIteratorNext(serialPortIterator);
        if (!serialPortService)
            break;

        QSerialPortInfoPrivate priv;

        QString calloutDevice;
        QString dialinDevice;

        for (;;) {
            if (calloutDevice.isEmpty())
                calloutDevice = calloutDeviceSystemLocation(serialPortService);

            if (dialinDevice.isEmpty())
                dialinDevice = dialinDeviceSystemLocation(serialPortService);

            if (priv.description.isEmpty())
                priv.description = deviceDescription(serialPortService);

            if (priv.manufacturer.isEmpty())
                priv.manufacturer = deviceManufacturer(serialPortService);

            if (priv.serialNumber.isEmpty())
                priv.serialNumber = deviceSerialNumber(serialPortService);

            if (!priv.hasVendorIdentifier) {
                priv.vendorIdentifier =
                        deviceVendorIdentifier(serialPortService,
                                               priv.hasVendorIdentifier);
            }

            if (!priv.hasProductIdentifier) {
                priv.productIdentifier =
                        deviceProductIdentifier(serialPortService,
                                                priv.hasProductIdentifier);
            }

            if (isCompleteInfo(priv, calloutDevice, dialinDevice)) {
                ::IOObjectRelease(serialPortService);
                break;
            }

            serialPortService = parentSerialPortService(serialPortService);
            if (!serialPortService)
                break;
        }

        QSerialPortInfoPrivate calloutCandidate = priv;
        calloutCandidate.device = calloutDevice;
        calloutCandidate.portName = QSerialPortInfoPrivate::portNameFromSystemLocation(calloutDevice);
        serialPortInfoList.append(calloutCandidate);

        QSerialPortInfoPrivate dialinCandidate = priv;
        dialinCandidate.device = dialinDevice;
        dialinCandidate.portName = QSerialPortInfoPrivate::portNameFromSystemLocation(dialinDevice);
        serialPortInfoList.append(dialinCandidate);
    }

    ::IOObjectRelease(serialPortIterator);

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
