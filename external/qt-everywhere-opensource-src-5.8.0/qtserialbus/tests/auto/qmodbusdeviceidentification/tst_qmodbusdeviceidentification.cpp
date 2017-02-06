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

#include <QtSerialBus/qmodbusdeviceidentification.h>
#include <QtSerialBus/qmodbuspdu.h>

#include <QtTest/QtTest>

class tst_QModbusDeviceIdentification : public QObject
{
    Q_OBJECT

private slots:
    void testConstructor()
    {
        QModbusDeviceIdentification qmdi;
        QCOMPARE(qmdi.isValid(), false);
        QCOMPARE(qmdi.objectIds().isEmpty(), true);
        for (int i = QModbusDeviceIdentification::VendorNameObjectId;
            i < QModbusDeviceIdentification::UndefinedObjectId; ++i) {
            QCOMPARE(qmdi.contains(i), false);
            qmdi.remove(i); // force crash if the behavior changes
            QCOMPARE(qmdi.value(i), QByteArray());
        }
        QCOMPARE(qmdi.conformityLevel(), QModbusDeviceIdentification::BasicConformityLevel);
    }

    void testIsValid()
    {
        QModbusDeviceIdentification qmdi;
        QCOMPARE(qmdi.isValid(), false);
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::ReservedObjectId, "Reserved"), true);
        QCOMPARE(qmdi.isValid(), false);
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::VendorNameObjectId,
            "Company identification"), true);
        QCOMPARE(qmdi.isValid(), false);
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::ProductCodeObjectId, "Product code"),
            true);
        QCOMPARE(qmdi.isValid(), false);
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::MajorMinorRevisionObjectId, "V2.11"),
            true);
        QCOMPARE(qmdi.isValid(), true);
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::MajorMinorRevisionObjectId, ""), true);
        QCOMPARE(qmdi.isValid(), false);
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::MajorMinorRevisionObjectId, "V2.11"),
            true);
        QCOMPARE(qmdi.isValid(), true);
    }

    void testRemoveAndContains()
    {
        QModbusDeviceIdentification qmdi;
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::ReservedObjectId), false);
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::ReservedObjectId, "Reserved"), true);
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::ReservedObjectId), true);
        qmdi.remove(QModbusDeviceIdentification::ReservedObjectId);
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::ReservedObjectId), false);
    }

    void testInsertAndValue()
    {
        QModbusDeviceIdentification qmdi;
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::ProductDependentObjectId, "Test"), true);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::ProductDependentObjectId),
            QByteArray("Test"));
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::ProductDependentObjectId,
            QByteArray(246, '@')), false);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::ProductDependentObjectId),
            QByteArray("Test"));
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::ProductDependentObjectId,
            QByteArray(245, '@')), true);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::ProductDependentObjectId),
            QByteArray(245, '@'));
        QCOMPARE(qmdi.insert(QModbusDeviceIdentification::UndefinedObjectId, "Test"), false);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::UndefinedObjectId), QByteArray());
    }

    void testConformityLevel()
    {
        QModbusDeviceIdentification qmdi;
        QCOMPARE(qmdi.conformityLevel(), QModbusDeviceIdentification::BasicConformityLevel);
        qmdi.setConformityLevel(QModbusDeviceIdentification::BasicIndividualConformityLevel);
        QCOMPARE(qmdi.conformityLevel(), QModbusDeviceIdentification::BasicIndividualConformityLevel);
    }

    void testConstructFromByteArray()
    {
        const QByteArray vendorNameObject = "Company identification";
        QModbusResponse r(QModbusResponse::EncapsulatedInterfaceTransport,
            QByteArray::fromHex("0e01010000010016") + vendorNameObject);

        {
        QModbusDeviceIdentification qmdi = QModbusDeviceIdentification::fromByteArray(r.data());
        QCOMPARE(qmdi.isValid(), false);
        QCOMPARE(qmdi.objectIds(), QList<int>() << QModbusDeviceIdentification::VendorNameObjectId);
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::VendorNameObjectId), true);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::VendorNameObjectId), vendorNameObject);
        QCOMPARE(qmdi.conformityLevel(), QModbusDeviceIdentification::BasicConformityLevel);
        }

        const QByteArray productCodeObject = "Product code";
        r.setData(QByteArray::fromHex("0e01010000020016") + vendorNameObject
            + QByteArray::fromHex("010c") + productCodeObject);
        {
        QModbusDeviceIdentification qmdi = QModbusDeviceIdentification::fromByteArray(r.data());
        QCOMPARE(qmdi.isValid(), false);
        QCOMPARE(qmdi.objectIds(), QList<int>() << QModbusDeviceIdentification::VendorNameObjectId
             << QModbusDeviceIdentification::ProductCodeObjectId);
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::VendorNameObjectId), true);
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::ProductCodeObjectId), true);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::VendorNameObjectId), vendorNameObject);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::ProductCodeObjectId), productCodeObject);
        QCOMPARE(qmdi.conformityLevel(), QModbusDeviceIdentification::BasicConformityLevel);
        }

        const QByteArray majorMinorRevision = "V2.11";
        r.setData(QByteArray::fromHex("0e01010000030016") + vendorNameObject
            + QByteArray::fromHex("010c") + productCodeObject + QByteArray::fromHex("0205")
            + majorMinorRevision);
        {
        QModbusDeviceIdentification qmdi = QModbusDeviceIdentification::fromByteArray(r.data());
        QCOMPARE(qmdi.isValid(), true);
        QCOMPARE(qmdi.objectIds(), QList<int>() << QModbusDeviceIdentification::VendorNameObjectId
             << QModbusDeviceIdentification::ProductCodeObjectId
             << QModbusDeviceIdentification::MajorMinorRevisionObjectId);
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::VendorNameObjectId), true);
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::ProductCodeObjectId), true);
        QCOMPARE(qmdi.contains(QModbusDeviceIdentification::MajorMinorRevisionObjectId), true);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::VendorNameObjectId), vendorNameObject);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::ProductCodeObjectId), productCodeObject);
        QCOMPARE(qmdi.value(QModbusDeviceIdentification::MajorMinorRevisionObjectId), majorMinorRevision);
        QCOMPARE(qmdi.conformityLevel(), QModbusDeviceIdentification::BasicConformityLevel);
        }
    }
};

QTEST_MAIN(tst_QModbusDeviceIdentification)

#include "tst_qmodbusdeviceidentification.moc"
