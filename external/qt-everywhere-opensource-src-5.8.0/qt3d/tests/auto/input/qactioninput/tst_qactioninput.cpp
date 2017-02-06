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
#include <Qt3DCore/qnodeid.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DInput/private/qactioninput_p.h>
#include <Qt3DInput/QActionInput>
#include <Qt3DInput/QAbstractPhysicalDevice>

#include "testpostmanarbiter.h"
#include "testdevice.h"

class tst_QActionInput: public QObject
{
    Q_OBJECT
public:
    tst_QActionInput()
    {
        qRegisterMetaType<Qt3DInput::QAbstractPhysicalDevice*>("Qt3DInput::QAbstractPhysicalDevice*");
    }

private Q_SLOTS:
    void checkCloning_data()
    {
        QTest::addColumn<Qt3DInput::QActionInput *>("actionInput");

        Qt3DInput::QActionInput *defaultConstructed = new Qt3DInput::QActionInput();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DInput::QActionInput *actionInputWithKeys = new Qt3DInput::QActionInput();
        actionInputWithKeys->setButtons(QVector<int>() << ((1 << 1) | (1 << 5)));
        QTest::newRow("actionInputWithKeys") << actionInputWithKeys;

        Qt3DInput::QActionInput *actionInputWithKeysAndSourceDevice = new Qt3DInput::QActionInput();
        TestDevice *device = new TestDevice();
        actionInputWithKeysAndSourceDevice->setButtons(QVector<int>() << ((1 << 1) | (1 << 5)));
        actionInputWithKeysAndSourceDevice->setSourceDevice(device);
        QTest::newRow("actionInputWithKeysAndSourceDevice") << actionInputWithKeysAndSourceDevice;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DInput::QActionInput *, actionInput);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(actionInput);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + (actionInput->sourceDevice() ? 1 : 0));

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DInput::QActionInputData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QActionInputData>>(creationChanges.first());
        const Qt3DInput::QActionInputData &cloneData = creationChangeData->data;

        // THEN
        QCOMPARE(actionInput->id(), creationChangeData->subjectId());
        QCOMPARE(actionInput->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(actionInput->metaObject(), creationChangeData->metaObject());
        QCOMPARE(actionInput->buttons(), cloneData.buttons);
        QCOMPARE(actionInput->sourceDevice() ? actionInput->sourceDevice()->id() : Qt3DCore::QNodeId(), cloneData.sourceDeviceId);
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QActionInput> actionInput(new Qt3DInput::QActionInput());
        arbiter.setArbiterOnNode(actionInput.data());

        // WHEN
        QVector<int> buttons = QVector<int>() << 555;
        actionInput->setButtons(buttons);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "buttons");
        QCOMPARE(change->value().value<QVector<int>>(), buttons);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        TestDevice *device = new TestDevice(actionInput.data());
        QCoreApplication::processEvents();
        arbiter.events.clear();

        actionInput->setSourceDevice(device);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "sourceDevice");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), device->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }

    void checkSourceDeviceBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DInput::QActionInput> actionInput(new Qt3DInput::QActionInput);
        {
            // WHEN
            Qt3DInput::QAbstractPhysicalDevice device;
            actionInput->setSourceDevice(&device);

            // THEN
            QCOMPARE(device.parent(), actionInput.data());
            QCOMPARE(actionInput->sourceDevice(), &device);
        }
        // THEN (Should not crash and effect be unset)
        QVERIFY(actionInput->sourceDevice() == nullptr);

        {
            // WHEN
            Qt3DInput::QActionInput someOtherActionInput;
            QScopedPointer<Qt3DInput::QAbstractPhysicalDevice> device(new Qt3DInput::QAbstractPhysicalDevice(&someOtherActionInput));
            actionInput->setSourceDevice(device.data());

            // THEN
            QCOMPARE(device->parent(), &someOtherActionInput);
            QCOMPARE(actionInput->sourceDevice(), device.data());

            // WHEN
            actionInput.reset();
            device.reset();

            // THEN Should not crash when the device is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QActionInput)

#include "tst_qactioninput.moc"
