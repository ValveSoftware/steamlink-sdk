/****************************************************************************
**
** Copyright (C) 2011-2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2011 Sergey Belyashov <Sergey.Belyashov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qserialportinfo.h"
#include "qserialportinfo_p.h"
#include "qserialport_win_p.h"

#include <QtCore/quuid.h>
#include <QtCore/qpair.h>
#include <QtCore/qstringlist.h>

#include <initguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>

QT_BEGIN_NAMESPACE

typedef QPair<QUuid, DWORD> GuidFlagsPair;

static inline const QList<GuidFlagsPair>& guidFlagsPairs()
{
    static const QList<GuidFlagsPair> guidFlagsPairList = QList<GuidFlagsPair>()
               // Standard Setup Ports Class GUID
            << qMakePair(QUuid(0x4D36E978, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18), DWORD(DIGCF_PRESENT))
               // Standard Setup Modems Class GUID
            << qMakePair(QUuid(0x4D36E96D, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18), DWORD(DIGCF_PRESENT))
               // Standard Serial Port Device Interface Class GUID
            << qMakePair(QUuid(0x86E0D1E0, 0x8089, 0x11D0, 0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73), DWORD(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE))
               // Standard Modem Device Interface Class GUID
            << qMakePair(QUuid(0x2C7089AA, 0x2E0E, 0x11D1, 0xB1, 0x14, 0x00, 0xC0, 0x4F, 0xC2, 0xAA, 0xE4), DWORD(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    return guidFlagsPairList;
}

static QString toStringAndTrimNullCharacter(const QByteArray &buffer)
{
    QString result = QString::fromWCharArray(reinterpret_cast<const wchar_t *>(buffer.constData()),
                                             buffer.size() / sizeof(wchar_t));
    while (!result.isEmpty() && (result.at(result.size() - 1).unicode() == 0))
        result.chop(1);
    return result;
}

static QStringList portNamesFromHardwareDeviceMap()
{
    HKEY hKey = Q_NULLPTR;
    if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return QStringList();

    QStringList result;
    DWORD index = 0;
    static const DWORD maximumValueNameInChars = 16383;
    QByteArray outputValueName;
    outputValueName.resize(maximumValueNameInChars * sizeof(wchar_t));
    QByteArray outputBuffer;
    DWORD requiredDataBytes = 0;
    forever {
        DWORD requiredValueNameChars = maximumValueNameInChars;
        const LONG ret = ::RegEnumValue(hKey, index, reinterpret_cast<wchar_t *>(outputValueName.data()), &requiredValueNameChars,
                                        Q_NULLPTR, Q_NULLPTR, reinterpret_cast<unsigned char *>(outputBuffer.data()), &requiredDataBytes);
        if (ret == ERROR_MORE_DATA) {
            outputBuffer.resize(requiredDataBytes);
        } else if (ret == ERROR_SUCCESS) {
            result.append(toStringAndTrimNullCharacter(outputBuffer));
            ++index;
        } else {
            break;
        }
    }
    ::RegCloseKey(hKey);
    return result;
}

static QString deviceRegistryProperty(HDEVINFO deviceInfoSet,
                                      PSP_DEVINFO_DATA deviceInfoData,
                                      DWORD property)
{
    DWORD dataType = 0;
    QByteArray devicePropertyByteArray;
    DWORD requiredSize = 0;
    forever {
        if (::SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, property, &dataType,
                                               reinterpret_cast<unsigned char *>(devicePropertyByteArray.data()),
                                               devicePropertyByteArray.size(), &requiredSize)) {
            break;
        }

        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER
                || (dataType != REG_SZ && dataType != REG_EXPAND_SZ)) {
            return QString();
        }
        devicePropertyByteArray.resize(requiredSize);
    }
    return toStringAndTrimNullCharacter(devicePropertyByteArray);
}

static QString deviceInstanceIdentifier(DEVINST deviceInstanceNumber)
{
    ULONG numberOfChars = 0;
    if (::CM_Get_Device_ID_Size(&numberOfChars, deviceInstanceNumber, 0) != CR_SUCCESS)
        return QString();
    // The size does not include the terminating null character.
    ++numberOfChars;
    QByteArray outputBuffer;
    outputBuffer.resize(numberOfChars * sizeof(wchar_t));
    if (::CM_Get_Device_ID(deviceInstanceNumber, reinterpret_cast<wchar_t *>(outputBuffer.data()),
                           outputBuffer.size(), 0) != CR_SUCCESS) {
        return QString();
    }
    return toStringAndTrimNullCharacter(outputBuffer).toUpper();
}

static DEVINST parentDeviceInstanceNumber(DEVINST childDeviceInstanceNumber)
{
    ULONG nodeStatus = 0;
    ULONG problemNumber = 0;
    if (::CM_Get_DevNode_Status(&nodeStatus, &problemNumber,
                                childDeviceInstanceNumber, 0) != CR_SUCCESS) {
        return 0;
    }
    DEVINST parentInstanceNumber = 0;
    if (::CM_Get_Parent(&parentInstanceNumber, childDeviceInstanceNumber, 0) != CR_SUCCESS)
        return 0;
    return parentInstanceNumber;
}

static QString devicePortName(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData)
{
    const HKEY key = ::SetupDiOpenDevRegKey(deviceInfoSet, deviceInfoData, DICS_FLAG_GLOBAL,
                                            0, DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE)
        return QString();

    static const QStringList portNameRegistryKeyList = QStringList()
            << QStringLiteral("PortName")
            << QStringLiteral("PortNumber");

    QString portName;
    foreach (const QString &portNameKey, portNameRegistryKeyList) {
        DWORD bytesRequired = 0;
        DWORD dataType = 0;
        QByteArray outputBuffer;
        forever {
            const LONG ret = ::RegQueryValueEx(key, reinterpret_cast<const wchar_t *>(portNameKey.utf16()), Q_NULLPTR, &dataType,
                                               reinterpret_cast<unsigned char *>(outputBuffer.data()), &bytesRequired);
            if (ret == ERROR_MORE_DATA) {
                outputBuffer.resize(bytesRequired);
                continue;
            } else if (ret == ERROR_SUCCESS) {
                if (dataType == REG_SZ)
                    portName = toStringAndTrimNullCharacter(outputBuffer);
                else if (dataType == REG_DWORD)
                    portName = QStringLiteral("COM%1").arg(*(PDWORD(outputBuffer.constData())));
            }
            break;
        }

        if (!portName.isEmpty())
            break;
    }
    ::RegCloseKey(key);
    return portName;
}

class SerialPortNameEqualFunctor
{
public:
    explicit SerialPortNameEqualFunctor(const QString &serialPortName)
        : m_serialPortName(serialPortName)
    {
    }

    bool operator() (const QSerialPortInfo &serialPortInfo) const
    {
        return serialPortInfo.portName() == m_serialPortName;
    }

private:
    const QString &m_serialPortName;
};

static QString deviceDescription(HDEVINFO deviceInfoSet,
                                 PSP_DEVINFO_DATA deviceInfoData)
{
    return deviceRegistryProperty(deviceInfoSet, deviceInfoData, SPDRP_DEVICEDESC);
}

static QString deviceManufacturer(HDEVINFO deviceInfoSet,
                                  PSP_DEVINFO_DATA deviceInfoData)
{
    return deviceRegistryProperty(deviceInfoSet, deviceInfoData, SPDRP_MFG);
}

static quint16 parseDeviceIdentifier(const QString &instanceIdentifier,
                                     const QString &identifierPrefix,
                                     int identifierSize, bool &ok)
{
    const int index = instanceIdentifier.indexOf(identifierPrefix);
    if (index == -1)
        return quint16(0);
    return instanceIdentifier.mid(index + identifierPrefix.size(), identifierSize).toInt(&ok, 16);
}

static quint16 deviceVendorIdentifier(const QString &instanceIdentifier, bool &ok)
{
    static const int vendorIdentifierSize = 4;
    quint16 result = parseDeviceIdentifier(
                instanceIdentifier, QStringLiteral("VID_"), vendorIdentifierSize, ok);
    if (!ok)
        result = parseDeviceIdentifier(
                    instanceIdentifier, QStringLiteral("VEN_"), vendorIdentifierSize, ok);
    return result;
}

static quint16 deviceProductIdentifier(const QString &instanceIdentifier, bool &ok)
{
    static const int productIdentifierSize = 4;
    quint16 result = parseDeviceIdentifier(
                instanceIdentifier, QStringLiteral("PID_"), productIdentifierSize, ok);
    if (!ok)
        result = parseDeviceIdentifier(
                    instanceIdentifier, QStringLiteral("DEV_"), productIdentifierSize, ok);
    return result;
}

static QString parseDeviceSerialNumber(const QString &instanceIdentifier)
{
    int firstbound = instanceIdentifier.lastIndexOf(QLatin1Char('\\'));
    int lastbound = instanceIdentifier.indexOf(QLatin1Char('_'), firstbound);
    if (instanceIdentifier.startsWith(QStringLiteral("USB\\"))) {
        if (lastbound != instanceIdentifier.size() - 3)
            lastbound = instanceIdentifier.size();
        int ampersand = instanceIdentifier.indexOf(QLatin1Char('&'), firstbound);
        if (ampersand != -1 && ampersand < lastbound)
            return QString();
    } else if (instanceIdentifier.startsWith(QStringLiteral("FTDIBUS\\"))) {
        firstbound = instanceIdentifier.lastIndexOf(QLatin1Char('+'));
        lastbound = instanceIdentifier.indexOf(QLatin1Char('\\'), firstbound);
        if (lastbound == -1)
            return QString();
    } else {
        return QString();
    }

    return instanceIdentifier.mid(firstbound + 1, lastbound - firstbound - 1);
}

static QString deviceSerialNumber(const QString &instanceIdentifier,
                                  DEVINST deviceInstanceNumber)
{
    QString result = parseDeviceSerialNumber(instanceIdentifier);
    if (result.isEmpty()) {
        const DEVINST parentNumber = parentDeviceInstanceNumber(deviceInstanceNumber);
        const QString parentInstanceIdentifier = deviceInstanceIdentifier(parentNumber);
        result = parseDeviceSerialNumber(parentInstanceIdentifier);
    }
    return result;
}

QList<QSerialPortInfo> QSerialPortInfo::availablePorts()
{
    QList<QSerialPortInfo> serialPortInfoList;

    foreach (const GuidFlagsPair &uniquePair, guidFlagsPairs()) {
        const HDEVINFO deviceInfoSet = ::SetupDiGetClassDevs(reinterpret_cast<const GUID *>(&uniquePair.first), Q_NULLPTR, Q_NULLPTR, uniquePair.second);
        if (deviceInfoSet == INVALID_HANDLE_VALUE)
            return serialPortInfoList;

        SP_DEVINFO_DATA deviceInfoData;
        ::memset(&deviceInfoData, 0, sizeof(deviceInfoData));
        deviceInfoData.cbSize = sizeof(deviceInfoData);

        DWORD index = 0;
        while (::SetupDiEnumDeviceInfo(deviceInfoSet, index++, &deviceInfoData)) {
            const QString portName = devicePortName(deviceInfoSet, &deviceInfoData);
            if (portName.isEmpty() || portName.contains(QStringLiteral("LPT")))
                continue;

            if (std::find_if(serialPortInfoList.begin(), serialPortInfoList.end(),
                             SerialPortNameEqualFunctor(portName)) != serialPortInfoList.end()) {
                continue;
            }

            QSerialPortInfoPrivate priv;

            priv.portName = portName;
            priv.device = QSerialPortInfoPrivate::portNameToSystemLocation(portName);
            priv.description = deviceDescription(deviceInfoSet, &deviceInfoData);
            priv.manufacturer = deviceManufacturer(deviceInfoSet, &deviceInfoData);

            const QString instanceIdentifier = deviceInstanceIdentifier(deviceInfoData.DevInst);

            priv.serialNumber =
                    deviceSerialNumber(instanceIdentifier, deviceInfoData.DevInst);
            priv.vendorIdentifier =
                    deviceVendorIdentifier(instanceIdentifier, priv.hasVendorIdentifier);
            priv.productIdentifier =
                    deviceProductIdentifier(instanceIdentifier, priv.hasProductIdentifier);

            serialPortInfoList.append(priv);
        }
        ::SetupDiDestroyDeviceInfoList(deviceInfoSet);
    }

    foreach (const QString &portName, portNamesFromHardwareDeviceMap()) {
        if (std::find_if(serialPortInfoList.begin(), serialPortInfoList.end(),
                         SerialPortNameEqualFunctor(portName)) == serialPortInfoList.end()) {
            QSerialPortInfoPrivate priv;
            priv.portName = portName;
            priv.device =  QSerialPortInfoPrivate::portNameToSystemLocation(portName);
            serialPortInfoList.append(priv);
        }
    }

    return serialPortInfoList;
}

QList<qint32> QSerialPortInfo::standardBaudRates()
{
    return QSerialPortPrivate::standardBaudRates();
}

bool QSerialPortInfo::isBusy() const
{
    const HANDLE handle = ::CreateFile(reinterpret_cast<const wchar_t*>(systemLocation().utf16()),
                                           GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, Q_NULLPTR);

    if (handle == INVALID_HANDLE_VALUE) {
        if (::GetLastError() == ERROR_ACCESS_DENIED)
            return true;
    } else {
        ::CloseHandle(handle);
    }
    return false;
}

bool QSerialPortInfo::isValid() const
{
    const HANDLE handle = ::CreateFile(reinterpret_cast<const wchar_t*>(systemLocation().utf16()),
                                           GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, Q_NULLPTR);

    if (handle == INVALID_HANDLE_VALUE) {
        if (::GetLastError() != ERROR_ACCESS_DENIED)
            return false;
    } else {
        ::CloseHandle(handle);
    }
    return true;
}

QString QSerialPortInfoPrivate::portNameToSystemLocation(const QString &source)
{
    return source.startsWith(QStringLiteral("COM"))
            ? (QStringLiteral("\\\\.\\") + source) : source;
}

QString QSerialPortInfoPrivate::portNameFromSystemLocation(const QString &source)
{
    return (source.startsWith(QStringLiteral("\\\\.\\"))
            || source.startsWith(QStringLiteral("//./")))
            ? source.mid(4) : source;
}

QT_END_NAMESPACE
