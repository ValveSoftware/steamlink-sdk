/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2013 Laszlo Papp <lpapp@kde.org>
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

#include <QtTest/QtTest>
#include <QtCore/QDebug>

#include <QtSerialPort/qserialportinfo.h>
#include <QtSerialPort/qserialport.h>

QT_USE_NAMESPACE

class tst_QSerialPortInfo : public QObject
{
    Q_OBJECT

private slots:
    void serialPortInfoList();
    void standardBaudRateList();
    void constructors();
    void assignment();
};

void tst_QSerialPortInfo::serialPortInfoList()
{
    QList<QSerialPortInfo> list(QSerialPortInfo::availablePorts());
    QCOMPARE(list.isEmpty(), false);
}

void tst_QSerialPortInfo::standardBaudRateList()
{
    QList<qint32> list(QSerialPortInfo::standardBaudRates());
    QCOMPARE(list.isEmpty(), false);
}

void tst_QSerialPortInfo::constructors()
{
    QSerialPortInfo serialPortInfo;
    QCOMPARE(serialPortInfo.portName().isEmpty(), true);
    QCOMPARE(serialPortInfo.systemLocation().isEmpty(), true);
    QCOMPARE(serialPortInfo.description().isEmpty(), true);
    QCOMPARE(serialPortInfo.manufacturer().isEmpty(), true);
    QCOMPARE(serialPortInfo.serialNumber().isEmpty(), true);
    QCOMPARE(serialPortInfo.vendorIdentifier(), quint16(0));
    QCOMPARE(serialPortInfo.productIdentifier(), quint16(0));
    QCOMPARE(serialPortInfo.hasVendorIdentifier(), false);
    QCOMPARE(serialPortInfo.hasProductIdentifier(), false);
    QCOMPARE(serialPortInfo.isNull(), false);
    QCOMPARE(serialPortInfo.isBusy(), false);
    QCOMPARE(serialPortInfo.isValid(), false);
}

void tst_QSerialPortInfo::assignment()
{
    QList<QSerialPortInfo> serialPortInfoList(QSerialPortInfo::availablePorts());

    foreach (const QSerialPortInfo &serialPortInfo, serialPortInfoList) {
        QSerialPortInfo otherSerialPortInfo = serialPortInfo;
        QCOMPARE(otherSerialPortInfo.portName(), serialPortInfo.portName());
        QCOMPARE(otherSerialPortInfo.systemLocation(), serialPortInfo.systemLocation());
        QCOMPARE(otherSerialPortInfo.description(), serialPortInfo.description());
        QCOMPARE(otherSerialPortInfo.manufacturer(), serialPortInfo.manufacturer());
        QCOMPARE(otherSerialPortInfo.serialNumber(), serialPortInfo.serialNumber());
        QCOMPARE(otherSerialPortInfo.vendorIdentifier(), serialPortInfo.vendorIdentifier());
        QCOMPARE(otherSerialPortInfo.productIdentifier(), serialPortInfo.productIdentifier());
    }
}

QTEST_MAIN(tst_QSerialPortInfo)
#include "tst_qserialportinfo.moc"
