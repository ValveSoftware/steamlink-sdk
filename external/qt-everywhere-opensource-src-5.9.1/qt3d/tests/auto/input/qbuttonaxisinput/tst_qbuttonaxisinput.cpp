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
#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DInput/QAbstractPhysicalDevice>
#include <Qt3DInput/QButtonAxisInput>
#include <Qt3DInput/private/qbuttonaxisinput_p.h>

#include "testpostmanarbiter.h"
#include "testdevice.h"

class tst_QButtonAxisInput: public QObject
{
    Q_OBJECT
public:
    tst_QButtonAxisInput()
    {
        qRegisterMetaType<Qt3DInput::QAbstractPhysicalDevice*>("Qt3DInput::QAbstractPhysicalDevice*");
    }

private Q_SLOTS:
    void shouldHaveDefaultState()
    {
        // GIVEN
        Qt3DInput::QButtonAxisInput axisInput;

        // THEN
        QVERIFY(axisInput.buttons().isEmpty());
        QCOMPARE(axisInput.scale(), 1.0f);
        QCOMPARE(axisInput.acceleration(), -1.0f);
        QCOMPARE(axisInput.deceleration(), -1.0f);
    }

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DInput::QButtonAxisInput *>("axisInput");

        Qt3DInput::QButtonAxisInput *defaultConstructed = new Qt3DInput::QButtonAxisInput();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DInput::QButtonAxisInput *axisInputWithKeys = new Qt3DInput::QButtonAxisInput();
        axisInputWithKeys->setButtons(QVector<int>() << ((1 << 1) | (1 << 5)));
        axisInputWithKeys->setScale(327.0f);
        QTest::newRow("axisInputWithKeys") << axisInputWithKeys;

        Qt3DInput::QButtonAxisInput *axisInputWithKeysAndSourceDevice = new Qt3DInput::QButtonAxisInput();
        TestDevice *device = new TestDevice();
        axisInputWithKeysAndSourceDevice->setButtons(QVector<int>() << ((1 << 1) | (1 << 5)));
        axisInputWithKeysAndSourceDevice->setSourceDevice(device);
        axisInputWithKeysAndSourceDevice->setScale(355.0f);
        QTest::newRow("axisInputWithKeysAndSourceDevice") << axisInputWithKeysAndSourceDevice;

        Qt3DInput::QButtonAxisInput *axisInputWithAcceleration = new Qt3DInput::QButtonAxisInput();
        axisInputWithAcceleration->setAcceleration(41.0f);
        axisInputWithAcceleration->setDeceleration(42.0f);
        QTest::newRow("axisInputWithAcceleration") << axisInputWithAcceleration;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DInput::QButtonAxisInput *, axisInput);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(axisInput);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + (axisInput->sourceDevice() ? 1 : 0));

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DInput::QButtonAxisInputData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QButtonAxisInputData>>(creationChanges.first());
        const Qt3DInput::QButtonAxisInputData &cloneData = creationChangeData->data;
        QCOMPARE(axisInput->id(), creationChangeData->subjectId());
        QCOMPARE(axisInput->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(axisInput->metaObject(), creationChangeData->metaObject());
        QCOMPARE(axisInput->buttons(), cloneData.buttons);
        QCOMPARE(axisInput->scale(), cloneData.scale);
        QCOMPARE(axisInput->acceleration(), cloneData.acceleration);
        QCOMPARE(axisInput->deceleration(), cloneData.deceleration);
        QCOMPARE(axisInput->sourceDevice() ? axisInput->sourceDevice()->id() : Qt3DCore::QNodeId(), cloneData.sourceDeviceId);
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QButtonAxisInput> axisInput(new Qt3DInput::QButtonAxisInput());
        arbiter.setArbiterOnNode(axisInput.data());

        // WHEN
        QVector<int> buttons = QVector<int>() << 555;
        axisInput->setButtons(buttons);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "buttons");
        QCOMPARE(change->value().value<QVector<int>>(), buttons);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        axisInput->setScale(1340.0f);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "scale");
        QCOMPARE(change->value().toFloat(), 1340.0f);
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

        // WHEN
        axisInput->setAcceleration(42.0f);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "acceleration");
        QCOMPARE(change->value().toFloat(), 42.0f);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        axisInput->setDeceleration(43.0f);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "deceleration");
        QCOMPARE(change->value().toFloat(), 43.0f);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }
};

QTEST_MAIN(tst_QButtonAxisInput)

#include "tst_qbuttonaxisinput.moc"
