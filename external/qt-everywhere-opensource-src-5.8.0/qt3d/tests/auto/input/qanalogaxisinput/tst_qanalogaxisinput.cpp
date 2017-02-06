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

#include <Qt3DInput/QAnalogAxisInput>
#include <Qt3DInput/QAbstractPhysicalDevice>
#include <Qt3DInput/private/qanalogaxisinput_p.h>

#include "testpostmanarbiter.h"
#include "testdevice.h"

class tst_QAnalogAxisInput: public QObject
{
    Q_OBJECT
public:
    tst_QAnalogAxisInput()
    {
        qRegisterMetaType<Qt3DInput::QAbstractPhysicalDevice*>("Qt3DInput::QAbstractPhysicalDevice*");
    }

private Q_SLOTS:
    void checkCloning_data()
    {
        QTest::addColumn<Qt3DInput::QAnalogAxisInput *>("axisInput");

        Qt3DInput::QAnalogAxisInput *defaultConstructed = new Qt3DInput::QAnalogAxisInput();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DInput::QAnalogAxisInput *axisInputWithAxis = new Qt3DInput::QAnalogAxisInput();
        axisInputWithAxis->setAxis(383);
        QTest::newRow("axisInputWithAxis") << axisInputWithAxis;

        Qt3DInput::QAnalogAxisInput *axisInputWithAxisAndSourceDevice = new Qt3DInput::QAnalogAxisInput();
        TestDevice *device = new TestDevice();
        axisInputWithAxisAndSourceDevice->setSourceDevice(device);
        axisInputWithAxisAndSourceDevice->setAxis(427);
        QTest::newRow("axisInputWithAxisAndSourceDevice") << axisInputWithAxisAndSourceDevice;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DInput::QAnalogAxisInput *, axisInput);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(axisInput);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + (axisInput->sourceDevice() ? 1 : 0));

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DInput::QAnalogAxisInputData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QAnalogAxisInputData>>(creationChanges.first());
        const Qt3DInput::QAnalogAxisInputData &cloneData = creationChangeData->data;
        QCOMPARE(axisInput->id(), creationChangeData->subjectId());
        QCOMPARE(axisInput->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(axisInput->metaObject(), creationChangeData->metaObject());
        QCOMPARE(axisInput->axis(), cloneData.axis);
        QCOMPARE(axisInput->sourceDevice() ? axisInput->sourceDevice()->id() : Qt3DCore::QNodeId(), cloneData.sourceDeviceId);
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QAnalogAxisInput> axisInput(new Qt3DInput::QAnalogAxisInput());
        arbiter.setArbiterOnNode(axisInput.data());

        // WHEN
        axisInput->setAxis(350);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "axis");
        QCOMPARE(change->value().toInt(), 350);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        TestDevice *device = new TestDevice(axisInput.data());
        QCoreApplication::processEvents();
        arbiter.events.clear();

        axisInput->setSourceDevice(device);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "sourceDevice");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), device->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }
};

QTEST_MAIN(tst_QAnalogAxisInput)

#include "tst_qanalogaxisinput.moc"
