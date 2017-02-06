/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

#include <QDebug>
#include <QMap>

#include <qbluetoothtransferrequest.h>
#include <qbluetoothaddress.h>
#include <qbluetoothlocaldevice.h>

QT_USE_NAMESPACE

typedef QMap<int,QVariant> tst_QBluetoothTransferRequest_QParameterMap;
Q_DECLARE_METATYPE(tst_QBluetoothTransferRequest_QParameterMap)

class tst_QBluetoothTransferRequest : public QObject
{
    Q_OBJECT

public:
    tst_QBluetoothTransferRequest();
    ~tst_QBluetoothTransferRequest();

private slots:
    void initTestCase();

    void tst_construction_data();
    void tst_construction();

    void tst_assignment_data();
    void tst_assignment();
};

tst_QBluetoothTransferRequest::tst_QBluetoothTransferRequest()
{
}

tst_QBluetoothTransferRequest::~tst_QBluetoothTransferRequest()
{
}

void tst_QBluetoothTransferRequest::initTestCase()
{
    // start Bluetooth if not started
    QBluetoothLocalDevice *device = new QBluetoothLocalDevice();
    device->powerOn();
    delete device;
}

void tst_QBluetoothTransferRequest::tst_construction_data()
{
    QTest::addColumn<QBluetoothAddress>("address");
    QTest::addColumn<QMap<int, QVariant> >("parameters");

    QMap<int, QVariant> inparameters;
    inparameters.insert((int)QBluetoothTransferRequest::DescriptionAttribute, "Desciption");
    inparameters.insert((int)QBluetoothTransferRequest::LengthAttribute, QVariant(1024));
    inparameters.insert((int)QBluetoothTransferRequest::TypeAttribute, "OPP");

    QTest::newRow("0x000000 COD") << QBluetoothAddress("000000000000") << inparameters;
    QTest::newRow("0x000100 COD") << QBluetoothAddress("000000000000") << inparameters;
    QTest::newRow("0x000104 COD") << QBluetoothAddress("000000000000") << inparameters;
    QTest::newRow("0x000118 COD") << QBluetoothAddress("000000000000") << inparameters;
    QTest::newRow("0x000200 COD") << QBluetoothAddress("000000000000") << inparameters;
}

void tst_QBluetoothTransferRequest::tst_construction()
{
    QFETCH(QBluetoothAddress, address);
    QFETCH(tst_QBluetoothTransferRequest_QParameterMap, parameters);

    QBluetoothTransferRequest transferRequest(address);

    foreach (int key, parameters.keys()) {
        transferRequest.setAttribute((QBluetoothTransferRequest::Attribute)key, parameters[key]);
        QCOMPARE(parameters[key], transferRequest.attribute((QBluetoothTransferRequest::Attribute)key));
    }

    QCOMPARE(transferRequest.address(), address);

    QBluetoothTransferRequest copyRequest(transferRequest);

    QCOMPARE(copyRequest.address(), address);
    QCOMPARE(transferRequest, copyRequest);
}

void tst_QBluetoothTransferRequest::tst_assignment_data()
{
    tst_construction_data();
}

void tst_QBluetoothTransferRequest::tst_assignment()
{
    QFETCH(QBluetoothAddress, address);
    QFETCH(tst_QBluetoothTransferRequest_QParameterMap, parameters);

    QBluetoothTransferRequest transferRequest(address);

    foreach (int key, parameters.keys()) {
        transferRequest.setAttribute((QBluetoothTransferRequest::Attribute)key, parameters[key]);
    }

    {
        QBluetoothTransferRequest copyRequest = transferRequest;

        QCOMPARE(copyRequest.address(), address);
        QCOMPARE(transferRequest, copyRequest);
    }

    {
        QBluetoothTransferRequest copyRequest(QBluetoothAddress("000000000001"));

        copyRequest = transferRequest;

        QCOMPARE(copyRequest.address(), address);
        QCOMPARE(transferRequest, copyRequest);
    }

    {
        QBluetoothTransferRequest copyRequest1(QBluetoothAddress("000000000001"));
        QBluetoothTransferRequest copyRequest2(QBluetoothAddress("000000000001"));

        copyRequest1 = copyRequest2 = transferRequest;

        QCOMPARE(copyRequest1.address(), address);
        QCOMPARE(copyRequest2.address(), address);
        QCOMPARE(transferRequest, copyRequest1);
        QCOMPARE(transferRequest, copyRequest2);

    }
}

QTEST_MAIN(tst_QBluetoothTransferRequest)

#include "tst_qbluetoothtransferrequest.moc"
