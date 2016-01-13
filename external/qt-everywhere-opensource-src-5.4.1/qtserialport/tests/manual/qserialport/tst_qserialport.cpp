/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
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

class tst_QSerialPort : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void open();
    void baudRate();
    void dataBits();
    void parity();
    void stopBits();
    void flowControl();

private:
    QList<QSerialPortInfo> serialPortInfoList;
};

void tst_QSerialPort::initTestCase()
{
    serialPortInfoList = QSerialPortInfo::availablePorts();

    if (serialPortInfoList.isEmpty()) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        QSKIP("Test doesn't work because the serial ports are not detected.");
#else
        QSKIP("Test doesn't work because the serial ports are not detected.", SkipAll);
#endif
    }
}

void tst_QSerialPort::open()
{
    foreach (const QSerialPortInfo &serialPortInfo, serialPortInfoList) {

        if (serialPortInfo.isBusy())
            continue;

        QSerialPort object1;

        // Try open and check access to port by Info
        object1.setPort(serialPortInfo);
        QCOMPARE(object1.portName(), serialPortInfo.portName());
        QCOMPARE(object1.open(QIODevice::ReadWrite), true);
        QCOMPARE(object1.isOpen(), true);
        object1.close();
        QCOMPARE(object1.isOpen(), false);

        // Try open and check access to port by Name
        object1.setPortName(serialPortInfo.portName());
        QCOMPARE(object1.portName(), serialPortInfo.portName());
        QCOMPARE(object1.open(QIODevice::ReadWrite), true);
        QCOMPARE(object1.isOpen(), true);
        object1.close();
        QCOMPARE(object1.isOpen(), false);

        // Try open and check access to port by Location
        object1.setPortName(serialPortInfo.systemLocation());
        QCOMPARE(object1.portName(), serialPortInfo.portName());
        QCOMPARE(object1.open(QIODevice::ReadWrite), true);
        QCOMPARE(object1.isOpen(), true);
        object1.close();
        QCOMPARE(object1.isOpen(), false);
    }
}

void tst_QSerialPort::baudRate()
{
    foreach (const QSerialPortInfo &serialPortInfo, serialPortInfoList) {

        QSerialPort object1;
        object1.setPortName(serialPortInfo.portName());
        QCOMPARE(object1.open(QIODevice::ReadWrite), true);

        QCOMPARE(object1.setBaudRate(QSerialPort::Baud1200), true);
        QCOMPARE(object1.baudRate(), static_cast<qint32>(QSerialPort::Baud1200));
        QCOMPARE(object1.setBaudRate(QSerialPort::Baud2400), true);
        QCOMPARE(object1.baudRate(), static_cast<qint32>(QSerialPort::Baud2400));
        QCOMPARE(object1.setBaudRate(QSerialPort::Baud4800), true);
        QCOMPARE(object1.baudRate(), static_cast<qint32>(QSerialPort::Baud4800));
        QCOMPARE(object1.setBaudRate(QSerialPort::Baud9600), true);
        QCOMPARE(object1.baudRate(), static_cast<qint32>(QSerialPort::Baud9600));
        QCOMPARE(object1.setBaudRate(QSerialPort::Baud19200), true);
        QCOMPARE(object1.baudRate(), static_cast<qint32>(QSerialPort::Baud19200));
        QCOMPARE(object1.setBaudRate(QSerialPort::Baud38400), true);
        QCOMPARE(object1.baudRate(), static_cast<qint32>(QSerialPort::Baud38400));
        QCOMPARE(object1.setBaudRate(QSerialPort::Baud57600), true);
        QCOMPARE(object1.baudRate(), static_cast<qint32>(QSerialPort::Baud57600));
        QCOMPARE(object1.setBaudRate(QSerialPort::Baud115200), true);
        QCOMPARE(object1.baudRate(), static_cast<qint32>(QSerialPort::Baud115200));

        object1.close();
    }
}

void tst_QSerialPort::dataBits()
{
    foreach (const QSerialPortInfo &serialPortInfo, serialPortInfoList) {

        QSerialPort object1;
        object1.setPortName(serialPortInfo.portName());
        QCOMPARE(object1.open(QIODevice::ReadWrite), true);

        QCOMPARE(object1.setDataBits(QSerialPort::Data8), true);
        QCOMPARE(object1.dataBits(), QSerialPort::Data8);

        object1.close();
    }
}

void tst_QSerialPort::parity()
{
    foreach (const QSerialPortInfo &serialPortInfo, serialPortInfoList) {

        QSerialPort object1;
        object1.setPortName(serialPortInfo.portName());
        QCOMPARE(object1.open(QIODevice::ReadWrite), true);

        QCOMPARE(object1.setParity(QSerialPort::NoParity), true);
        QCOMPARE(object1.parity(), QSerialPort::NoParity);
        QCOMPARE(object1.setParity(QSerialPort::EvenParity), true);
        QCOMPARE(object1.parity(), QSerialPort::EvenParity);
        QCOMPARE(object1.setParity(QSerialPort::OddParity), true);
        QCOMPARE(object1.parity(), QSerialPort::OddParity);
        QCOMPARE(object1.setParity(QSerialPort::MarkParity), true);
        QCOMPARE(object1.parity(), QSerialPort::MarkParity);
        QCOMPARE(object1.setParity(QSerialPort::SpaceParity), true);
        QCOMPARE(object1.parity(), QSerialPort::SpaceParity);

        object1.close();
    }
}

void tst_QSerialPort::stopBits()
{
    foreach (const QSerialPortInfo &serialPortInfo, serialPortInfoList) {

        QSerialPort object1;
        object1.setPortName(serialPortInfo.portName());
        QCOMPARE(object1.open(QIODevice::ReadWrite), true);

        QCOMPARE(object1.setStopBits(QSerialPort::OneStop), true);
        QCOMPARE(object1.stopBits(), QSerialPort::OneStop);
        // skip 1.5 stop bits
        QCOMPARE(object1.setStopBits(QSerialPort::TwoStop), true);
        QCOMPARE(object1.stopBits(), QSerialPort::TwoStop);

        object1.close();
    }
}

void tst_QSerialPort::flowControl()
{
    foreach (const QSerialPortInfo &serialPortInfo, serialPortInfoList) {

        QSerialPort object1;
        object1.setPortName(serialPortInfo.portName());
        QCOMPARE(object1.open(QIODevice::ReadWrite), true);

        QCOMPARE(object1.setFlowControl(QSerialPort::NoFlowControl), true);
        QCOMPARE(object1.flowControl(), QSerialPort::NoFlowControl);
        QCOMPARE(object1.setFlowControl(QSerialPort::HardwareControl), true);
        QCOMPARE(object1.flowControl(), QSerialPort::HardwareControl);
        QCOMPARE(object1.setFlowControl(QSerialPort::SoftwareControl), true);
        QCOMPARE(object1.flowControl(), QSerialPort::SoftwareControl);

        object1.close();
    }
}

QTEST_MAIN(tst_QSerialPort)
#include "tst_qserialport.moc"
