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
#include "qserialport_symbian_p.h"

#include <e32base.h>
#include <c32comm.h>
#include <f32file.h>

#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

#ifdef __WINS__
_LIT(KPddName, "ECDRV");
#else
_LIT(KPddName, "EUART");
#endif

_LIT(KLddName,"ECOMM");

_LIT(KRS232ModuleName , "ECUART");
_LIT(KBluetoothModuleName , "BTCOMM");
_LIT(KInfraRedModuleName , "IRCOMM");
_LIT(KACMModuleName, "ECACM");

static bool loadDevices()
{
    TInt r = KErrNone;
#ifdef __WINS__
    RFs fileServer;
    r = User::LeaveIfError(fileServer.Connect());
    if (r != KErrNone)
        return false;
    fileServer.Close ();
#endif

    r = User::LoadPhysicalDevice(KPddName);
    if (r != KErrNone && r != KErrAlreadyExists)
        return false;

    r = User::LoadLogicalDevice(KLddName);
    if (r != KErrNone && r != KErrAlreadyExists)
        return false;

#ifndef __WINS__
    r = StartC32();
    if (r != KErrNone && r != KErrAlreadyExists)
        return false;
#endif

    return true;
}

QList<QSerialPortInfo> QSerialPortInfo::availablePorts()
{
    QList<QSerialPortInfo> serialPortInfoList;

    if (!loadDevices())
        return serialPortInfoList;

    RCommServ server;
    TInt r = server.Connect();
    if (r != KErrNone)
        return serialPortInfoList;

    TSerialInfo nativeSerialInfo;
    QString s("%1::%2");

    // FIXME: Get info about RS232 ports.
    r = server.LoadCommModule(KRS232ModuleName);
    if (r == KErrNone) {
        r = server.GetPortInfo(KRS232ModuleName, nativeSerialInfo);
        if (r == KErrNone) {
            for (quint32 i = nativeSerialInfo.iLowUnit; i < nativeSerialInfo.iHighUnit + 1; ++i) {

                QSerialPortInfo serialPortInfo;

                serialPortInfo.d_ptr->device = s
                        .arg(QString::fromUtf16(nativeSerialInfo.iName.Ptr(), nativeSerialInfo.iName.Length()))
                        .arg(i);
                serialPortInfo.d_ptr->portName = serialPortInfo.d_ptr->device;
                serialPortInfo.d_ptr->description =
                        QString::fromUtf16(nativeSerialInfo.iDescription.Ptr(), nativeSerialInfo.iDescription.Length());
                serialPortInfo.d_ptr->manufacturer = QString(QObject::tr("Unknown."));
                serialPortInfo.d_ptr->serialNumber = QString(QObject::tr("Unknown."));
                serialPortInfoList.append(serialPortInfo);
            }
        }
    }

    // FIXME: Get info about Bluetooth ports.
    r = server.LoadCommModule(KBluetoothModuleName);
    if (r == KErrNone) {
        r = server.GetPortInfo(KBluetoothModuleName, nativeSerialInfo);
        if (r == KErrNone) {
            for (quint32 i = nativeSerialInfo.iLowUnit; i < nativeSerialInfo.iHighUnit + 1; ++i) {

                QSerialPortInfo serialPortInfo;

                serialPortInfo.d_ptr->device = s
                        .arg(QString::fromUtf16(nativeSerialInfo.iName.Ptr(), nativeSerialInfo.iName.Length()))
                        .arg(i);
                serialPortInfo.d_ptr->portName = serialPortInfo.d_ptr->device;
                serialPortInfo.d_ptr->description =
                        QString::fromUtf16(nativeSerialInfo.iDescription.Ptr(), nativeSerialInfo.iDescription.Length());
                serialPortInfo.d_ptr->manufacturer = QString(QObject::tr("Unknown."));
                serialPortInfo.d_ptr->serialNumber = QString(QObject::tr("Unknown."));
                serialPortInfoList.append(serialPortInfo);
            }
        }
    }

    // FIXME: Get info about InfraRed ports.
    r = server.LoadCommModule(KInfraRedModuleName);
    if (r == KErrNone) {
        r = server.GetPortInfo(KInfraRedModuleName, nativeSerialInfo);
        if (r == KErrNone) {
            for (quint32 i = nativeSerialInfo.iLowUnit; i < nativeSerialInfo.iHighUnit + 1; ++i) {

                QSerialPortInfo serialPortInfo;

                serialPortInfo.d_ptr->device = s
                        .arg(QString::fromUtf16(nativeSerialInfo.iName.Ptr(), nativeSerialInfo.iName.Length()))
                        .arg(i);
                serialPortInfo.d_ptr->portName = serialPortInfo.d_ptr->device;
                serialPortInfo.d_ptr->description =
                        QString::fromUtf16(nativeSerialInfo.iDescription.Ptr(), nativeSerialInfo.iDescription.Length());
                serialPortInfo.d_ptr->manufacturer = QString(QObject::tr("Unknown."));
                serialPortInfoList.append(serialPortInfo);
            }
        }
    }

    // FIXME: Get info about ACM ports.
    r = server.LoadCommModule(KACMModuleName);
    if (r == KErrNone) {
        r = server.GetPortInfo(KACMModuleName, nativeSerialInfo);
        if (r == KErrNone) {
            for (quint32 i = nativeSerialInfo.iLowUnit; i < nativeSerialInfo.iHighUnit + 1; ++i) {

                QSerialPortInfo serialPortInfo;

                serialPortInfo.d_ptr->device = s
                        .arg(QString::fromUtf16(nativeSerialInfo.iName.Ptr(), nativeSerialInfo.iName.Length()))
                        .arg(i);
                serialPortInfo.d_ptr->portName = QSerialPortPrivate::portNameFromSystemLocation(serialPortInfo.d_ptr->device);
                serialPortInfo.d_ptr->description =
                        QString::fromUtf16(nativeSerialInfo.iDescription.Ptr(), nativeSerialInfo.iDescription.Length());
                serialPortInfo.d_ptr->manufacturer = QString(QObject::tr("Unknown."));
                serialPortInfoList.append(serialPortInfo);
            }
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
    if (!loadDevices())
        return false;

    RCommServ server;
    TInt r = server.Connect();
    if (r != KErrNone)
        return false;

    RComm port;
    TPtrC portName(static_cast<const TUint16*>(systemLocation().utf16()), systemLocation().length());
    r = port.Open(server, portName, ECommExclusive);
    if (r == KErrNone)
        port.Close();
    return r == KErrLocked;
}

bool QSerialPortInfo::isValid() const
{
    if (!loadDevices())
        return false;

    RCommServ server;
    TInt r = server.Connect();
    if (r != KErrNone)
        return false;

    RComm port;
    TPtrC portName(static_cast<const TUint16*>(systemLocation().utf16()), systemLocation().length());
    r = port.Open(server, portName, ECommExclusive);
    if (r == KErrNone)
        port.Close();
    return r == KErrNone || r == KErrLocked;
}

QT_END_NAMESPACE
