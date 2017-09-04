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

#include <Qt3DInput/QKeyboardDevice>
#include <Qt3DInput/QKeyboardHandler>
#include <Qt3DInput/private/qkeyboardhandler_p.h>

#include "testpostmanarbiter.h"

class tst_QKeyboardHandler : public QObject
{
    Q_OBJECT
public:
    tst_QKeyboardHandler()
    {
        qRegisterMetaType<Qt3DInput::QKeyboardDevice*>("Qt3DInput::QKeyboardDevice*");
    }

private Q_SLOTS:
    void checkCloning_data()
    {
        QTest::addColumn<Qt3DInput::QKeyboardHandler *>("keyboardHandler");

        auto defaultConstructed = new Qt3DInput::QKeyboardHandler;
        QTest::newRow("defaultConstructed") << defaultConstructed;

        auto handlerWithDevice = new Qt3DInput::QKeyboardHandler;
        handlerWithDevice->setSourceDevice(new Qt3DInput::QKeyboardDevice);
        QTest::newRow("handlerWithDevice") << handlerWithDevice;

        auto handlerWithDeviceAndFocus = new Qt3DInput::QKeyboardHandler;
        handlerWithDeviceAndFocus->setSourceDevice(new Qt3DInput::QKeyboardDevice);
        handlerWithDeviceAndFocus->setFocus(true);
        QTest::newRow("handlerWithDeviceAndFocus") << handlerWithDeviceAndFocus;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DInput::QKeyboardHandler *, keyboardHandler);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(keyboardHandler);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + (keyboardHandler->sourceDevice() ? 1 : 0));

        auto creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QKeyboardHandlerData>>(creationChanges.first());
        const Qt3DInput::QKeyboardHandlerData &cloneData = creationChangeData->data;
        QCOMPARE(keyboardHandler->id(), creationChangeData->subjectId());
        QCOMPARE(keyboardHandler->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(keyboardHandler->metaObject(), creationChangeData->metaObject());
        QCOMPARE(keyboardHandler->focus(), cloneData.focus);
        QCOMPARE(keyboardHandler->sourceDevice() ? keyboardHandler->sourceDevice()->id() : Qt3DCore::QNodeId(), cloneData.keyboardDeviceId);
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DInput::QKeyboardHandler> keyboardHandler(new Qt3DInput::QKeyboardHandler);
        arbiter.setArbiterOnNode(keyboardHandler.data());

        // WHEN
        keyboardHandler->setFocus(true);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "focus");
        QCOMPARE(change->value().toBool(), true);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        auto device = new Qt3DInput::QKeyboardDevice(keyboardHandler.data());
        QCoreApplication::processEvents();
        arbiter.events.clear();

        keyboardHandler->setSourceDevice(device);
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
        QScopedPointer<Qt3DInput::QKeyboardHandler> keyboardHandler(new Qt3DInput::QKeyboardHandler);
        {
            // WHEN
            Qt3DInput::QKeyboardDevice device;
            keyboardHandler->setSourceDevice(&device);

            // THEN
            QCOMPARE(device.parent(), keyboardHandler.data());
            QCOMPARE(keyboardHandler->sourceDevice(), &device);
        }
        // THEN (Should not crash and effect be unset)
        QVERIFY(keyboardHandler->sourceDevice() == nullptr);

        {
            // WHEN
            Qt3DInput::QKeyboardHandler someOtherKeyboardHandler;
            QScopedPointer<Qt3DInput::QKeyboardDevice> device(new Qt3DInput::QKeyboardDevice(&someOtherKeyboardHandler));
            keyboardHandler->setSourceDevice(device.data());

            // THEN
            QCOMPARE(device->parent(), &someOtherKeyboardHandler);
            QCOMPARE(keyboardHandler->sourceDevice(), device.data());

            // WHEN
            keyboardHandler.reset();
            device.reset();

            // THEN Should not crash when the device is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QKeyboardHandler)

#include "tst_qkeyboardhandler.moc"
