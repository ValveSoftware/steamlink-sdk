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
#include "qserialport_wince_p.h"

#include <QtCore/qstringlist.h>

QT_BEGIN_NAMESPACE

static QString findDescription(HKEY parentKeyHandle, const QString &subKey)
{
    const static QString valueName(QStringLiteral("FriendlyName"));

    QString result;
    HKEY hSubKey = Q_NULLPTR;
    LONG res = ::RegOpenKeyEx(parentKeyHandle, reinterpret_cast<const wchar_t *>(subKey.utf16()),
                              0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hSubKey);

    if (res == ERROR_SUCCESS) {

        DWORD dataType = 0;
        DWORD dataSize = 0;
        res = ::RegQueryValueEx(hSubKey, reinterpret_cast<const wchar_t *>(valueName.utf16()),
                                Q_NULLPTR, &dataType, Q_NULLPTR, &dataSize);

        if (res == ERROR_SUCCESS) {
            QByteArray data(dataSize, 0);
            res = ::RegQueryValueEx(hSubKey, reinterpret_cast<const wchar_t *>(valueName.utf16()),
                                    Q_NULLPTR, Q_NULLPTR,
                                    reinterpret_cast<unsigned char *>(data.data()),
                                    &dataSize);

            if (res == ERROR_SUCCESS) {
                switch (dataType) {
                case REG_EXPAND_SZ:
                case REG_SZ:
                    if (dataSize)
                        result = QString::fromWCharArray(reinterpret_cast<const wchar_t *>(data.constData()));
                    break;
                default:
                    break;
                }
            }
        } else {
            DWORD index = 0;
            dataSize = 255; // Max. key length (see MSDN).
            QByteArray data(dataSize, 0);
            while (::RegEnumKeyEx(hSubKey, index++,
                                  reinterpret_cast<wchar_t *>(data.data()), &dataSize,
                                  Q_NULLPTR, Q_NULLPTR, Q_NULLPTR, Q_NULLPTR) == ERROR_SUCCESS) {

                result = findDescription(hSubKey,
                                         QString::fromUtf16(reinterpret_cast<ushort *>(data.data()), dataSize));
                if (!result.isEmpty())
                    break;
            }
        }
        ::RegCloseKey(hSubKey);
    }
    return result;
}

QList<QSerialPortInfo> QSerialPortInfo::availablePorts()
{
    QList<QSerialPortInfo> serialPortInfoList;

    DEVMGR_DEVICE_INFORMATION di;
    di.dwSize = sizeof(di);
    const HANDLE hSearch = ::FindFirstDevice(DeviceSearchByLegacyName,
                                             L"COM*",
                                             &di);
    if (hSearch != INVALID_HANDLE_VALUE) {
        do {
            QSerialPortInfoPrivate priv;
            priv.device = QString::fromWCharArray(di.szLegacyName);
            priv.portName = QSerialPortInfoPrivate::portNameFromSystemLocation(priv.device);
            priv.description = findDescription(HKEY_LOCAL_MACHINE,
                                               QString::fromWCharArray(di.szDeviceKey));

            serialPortInfoList.append(priv);

        } while (::FindNextDevice(hSearch, &di));
        ::FindClose(hSearch);
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
                                           GENERIC_READ | GENERIC_WRITE, 0, Q_NULLPTR, OPEN_EXISTING, 0, Q_NULLPTR);

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
                                           GENERIC_READ | GENERIC_WRITE, 0, Q_NULLPTR, OPEN_EXISTING, 0, Q_NULLPTR);

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
    return source.endsWith(QLatin1Char(':'))
            ? source : (source + QLatin1Char(':'));
}

QString QSerialPortInfoPrivate::portNameFromSystemLocation(const QString &source)
{
    return source.endsWith(QLatin1Char(':'))
            ? source.mid(0, source.size() - 1) : source;
}

QT_END_NAMESPACE
