/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <QtTest/QTest>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DInput/QAbstractAxisInput>
#include <Qt3DInput/QAbstractPhysicalDevice>
#include <Qt3DInput/private/qabstractaxisinput_p.h>

#include "testpostmanarbiter.h"
#include "testdevice.h"

class DummyAxisInput : public Qt3DInput::QAbstractAxisInput
{
    Q_OBJECT
public:
    explicit DummyAxisInput(QNode *parent = nullptr)
        : Qt3DInput::QAbstractAxisInput(*new Qt3DInput::QAbstractAxisInputPrivate, parent)
    {
    }
};

class tst_QAbstractAxisInput: public QObject
{
    Q_OBJECT
public:
    tst_QAbstractAxisInput()
    {
        qRegisterMetaType<Qt3DInput::QAbstractPhysicalDevice*>("Qt3DInput::QAbstractPhysicalDevice*");
        qRegisterMetaType<Qt3DCore::QNode*>("Qt3DCore::QNode*");
    }

private Q_SLOTS:
    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QAbstractAxisInput> axisInput(new DummyAxisInput());
        arbiter.setArbiterOnNode(axisInput.data());

        // WHEN
        TestDevice *device = new TestDevice(axisInput.data());
        QCoreApplication::processEvents();
        arbiter.events.clear();

        axisInput->setSourceDevice(device);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "sourceDevice");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), device->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }

    void checkSourceDeviceBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DInput::QAbstractAxisInput> axisInput(new DummyAxisInput);
        {
            // WHEN
            Qt3DInput::QAbstractPhysicalDevice device;
            axisInput->setSourceDevice(&device);

            // THEN
            QCOMPARE(device.parent(), axisInput.data());
            QCOMPARE(axisInput->sourceDevice(), &device);
        }
        // THEN (Should not crash and effect be unset)
        QVERIFY(axisInput->sourceDevice() == nullptr);

        {
            // WHEN
            DummyAxisInput someOtherAxisInput;
            QScopedPointer<Qt3DInput::QAbstractPhysicalDevice> device(new Qt3DInput::QAbstractPhysicalDevice(&someOtherAxisInput));
            axisInput->setSourceDevice(device.data());

            // THEN
            QCOMPARE(device->parent(), &someOtherAxisInput);
            QCOMPARE(axisInput->sourceDevice(), device.data());

            // WHEN
            axisInput.reset();
            device.reset();

            // THEN Should not crash when the device is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QAbstractAxisInput)

#include "tst_qabstractaxisinput.moc"
