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

#include <Qt3DInput/QLogicalDevice>
#include <Qt3DInput/private/qlogicaldevice_p.h>
#include <Qt3DInput/QAxis>
#include <Qt3DInput/QAction>

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>

#include "testpostmanarbiter.h"

class tst_QLogicalDevice: public QObject
{
    Q_OBJECT
public:
    tst_QLogicalDevice()
    {
        qRegisterMetaType<Qt3DCore::QNode *>();
    }

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DInput::QLogicalDevice *>("logicalDevice");

        Qt3DInput::QLogicalDevice *defaultConstructed = new Qt3DInput::QLogicalDevice();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DInput::QLogicalDevice *logicalDeviceWithActions = new Qt3DInput::QLogicalDevice();
        logicalDeviceWithActions->addAction(new Qt3DInput::QAction());
        logicalDeviceWithActions->addAction(new Qt3DInput::QAction());
        logicalDeviceWithActions->addAction(new Qt3DInput::QAction());
        QTest::newRow("logicalDeviceWithActions") << logicalDeviceWithActions;

        Qt3DInput::QLogicalDevice *logicalDeviceWithAxes = new Qt3DInput::QLogicalDevice();
        logicalDeviceWithAxes->addAxis(new Qt3DInput::QAxis());
        logicalDeviceWithAxes->addAxis(new Qt3DInput::QAxis());
        logicalDeviceWithAxes->addAxis(new Qt3DInput::QAxis());
        QTest::newRow("logicalDeviceWithAxes") << logicalDeviceWithAxes;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DInput::QLogicalDevice *, logicalDevice);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(logicalDevice);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        const int axesCount = logicalDevice->axes().count();
        const int actionsCount = logicalDevice->actions().count();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + axesCount + actionsCount);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DInput::QLogicalDeviceData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QLogicalDeviceData>>(creationChanges.first());
        const Qt3DInput::QLogicalDeviceData &cloneData = creationChangeData->data;

        // THEN
        QCOMPARE(logicalDevice->id(), creationChangeData->subjectId());
        QCOMPARE(logicalDevice->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(logicalDevice->metaObject(), creationChangeData->metaObject());
        QCOMPARE(axesCount, cloneData.axisIds.count());
        QCOMPARE(actionsCount, cloneData.actionIds.count());

        for (int i = 0; i < axesCount; ++i)
            QCOMPARE(logicalDevice->axes().at(i)->id(), cloneData.axisIds.at(i));

        for (int i = 0; i < actionsCount; ++i)
            QCOMPARE(logicalDevice->actions().at(i)->id(), cloneData.actionIds.at(i));
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QLogicalDevice> logicalDevice(new Qt3DInput::QLogicalDevice());
        arbiter.setArbiterOnNode(logicalDevice.data());

        // WHEN
        Qt3DInput::QAction *action = new Qt3DInput::QAction(logicalDevice.data());
        QCoreApplication::processEvents();
        arbiter.events.clear();

        logicalDevice->addAction(action);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeAddedChangePtr nodeAddedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(nodeAddedChange->propertyName(), "action");
        QCOMPARE(nodeAddedChange->addedNodeId(), action->id());
        QCOMPARE(nodeAddedChange->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        logicalDevice->removeAction(action);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyNodeRemovedChangePtr nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "action");
        QCOMPARE(nodeRemovedChange->removedNodeId(), action->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

        arbiter.events.clear();

        // WHEN
        Qt3DInput::QAxis *axis = new Qt3DInput::QAxis(logicalDevice.data());
        QCoreApplication::processEvents();
        arbiter.events.clear();

        logicalDevice->addAxis(axis);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        nodeAddedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
        QCOMPARE(nodeAddedChange->propertyName(), "axis");
        QCOMPARE(nodeAddedChange->addedNodeId(), axis->id());
        QCOMPARE(nodeAddedChange->type(), Qt3DCore::PropertyValueAdded);

        arbiter.events.clear();

        // WHEN
        logicalDevice->removeAxis(axis);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "axis");
        QCOMPARE(nodeRemovedChange->removedNodeId(), axis->id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyValueRemoved);

        arbiter.events.clear();
    }

    void checkAxisBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DInput::QLogicalDevice> device(new Qt3DInput::QLogicalDevice);
        {
            // WHEN
            Qt3DInput::QAxis axis;
            device->addAxis(&axis);

            // THEN
            QCOMPARE(axis.parent(), device.data());
            QCOMPARE(device->axes().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(device->axes().empty());

        {
            // WHEN
            Qt3DInput::QLogicalDevice someOtherDevice;
            QScopedPointer<Qt3DInput::QAxis> axis(new Qt3DInput::QAxis(&someOtherDevice));
            device->addAxis(axis.data());

            // THEN
            QCOMPARE(axis->parent(), &someOtherDevice);
            QCOMPARE(device->axes().size(), 1);

            // WHEN
            device.reset();
            axis.reset();

            // THEN Should not crash when the axis is destroyed (tests for failed removal of destruction helper)
        }
    }

    void checkActionBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DInput::QLogicalDevice> device(new Qt3DInput::QLogicalDevice);
        {
            // WHEN
            Qt3DInput::QAction action;
            device->addAction(&action);

            // THEN
            QCOMPARE(action.parent(), device.data());
            QCOMPARE(device->actions().size(), 1);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(device->actions().empty());

        {
            // WHEN
            Qt3DInput::QLogicalDevice someOtherDevice;
            QScopedPointer<Qt3DInput::QAction> action(new Qt3DInput::QAction(&someOtherDevice));
            device->addAction(action.data());

            // THEN
            QCOMPARE(action->parent(), &someOtherDevice);
            QCOMPARE(device->actions().size(), 1);

            // WHEN
            device.reset();
            action.reset();

            // THEN Should not crash when the action is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QLogicalDevice)

#include "tst_qlogicaldevice.moc"
