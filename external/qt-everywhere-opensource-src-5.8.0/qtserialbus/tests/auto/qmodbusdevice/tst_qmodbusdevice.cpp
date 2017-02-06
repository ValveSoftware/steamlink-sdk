/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtSerialBus/qmodbusdevice.h>

class DummyDevice : public QModbusDevice
{
    Q_OBJECT
    friend class tst_QModbusDevice;

public:
    DummyDevice()
    {
        qRegisterMetaType<QModbusDevice::State>("QModbusDevice::State");
    }

protected:
    bool open() override { return openState; }
    void close() override {}

    bool openState = false;
};

class tst_QModbusDevice : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void connectDevice();
    void state();
    void error();

private:
    DummyDevice *device;
};

void tst_QModbusDevice::initTestCase()
{
    device = new DummyDevice();
}

void tst_QModbusDevice::cleanupTestCase()
{
    delete device;
}

void tst_QModbusDevice::connectDevice()
{
    device->openState = false;
    QVERIFY(!device->connectDevice());
    QCOMPARE(device->state(), QModbusDevice::UnconnectedState);

    device->openState = true;
    QVERIFY(device->connectDevice());
    QCOMPARE(device->state(), QModbusDevice::ConnectingState);

    device->disconnectDevice();
    QCOMPARE(device->state(), QModbusDevice::ClosingState);

    device->setState(QModbusDevice::ClosingState);
    QVERIFY(!device->connectDevice());
}

void tst_QModbusDevice::state()
{
    device->setState(QModbusDevice::ConnectedState);
    QCOMPARE(device->state(), QModbusDevice::ConnectedState);
    QSignalSpy spy(device, SIGNAL(stateChanged(QModbusDevice::State)));
    device->setState(QModbusDevice::UnconnectedState);
    QCOMPARE(device->state(), QModbusDevice::UnconnectedState);
    device->setState(QModbusDevice::UnconnectedState);
    QCOMPARE(device->state(), QModbusDevice::UnconnectedState);

    QCOMPARE(spy.count(), 1);
}

void tst_QModbusDevice::error()
{
    QCOMPARE(device->error(), QModbusDevice::NoError);
    QVERIFY(device->errorString().isEmpty());

    QString errorString("error string");
    device->setError(errorString, QModbusDevice::ConnectionError);

    QCOMPARE(device->error(), QModbusDevice::ConnectionError);
    QCOMPARE(device->errorString(), errorString);
}

QTEST_MAIN(tst_QModbusDevice)

#include "tst_qmodbusdevice.moc"
