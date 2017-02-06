/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DInput/qmousedevice.h>
#include <Qt3DInput/private/qmousedevice_p.h>
#include <Qt3DInput/private/mousedevice_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "qbackendnodetester.h"

class tst_MouseDevice : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DInput::Input::MouseDevice backendMouseDevice;

        // THEN
        QCOMPARE(backendMouseDevice.isEnabled(), false);
        QVERIFY(backendMouseDevice.peerId().isNull());
        QVERIFY(backendMouseDevice.inputHandler() == nullptr);
        QCOMPARE(backendMouseDevice.mouseState().xAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().yAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wXAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wYAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().leftPressed, false);
        QCOMPARE(backendMouseDevice.mouseState().rightPressed, false);
        QCOMPARE(backendMouseDevice.mouseState().centerPressed, false);
        QCOMPARE(backendMouseDevice.previousPos(), QPointF());
        QCOMPARE(backendMouseDevice.wasPressed(), false);
        QCOMPARE(backendMouseDevice.sensitivity(), 0.1f);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DInput::QMouseDevice mouseDevice;
        mouseDevice.setSensitivity(0.8f);

        {
            // WHEN
            Qt3DInput::Input::MouseDevice backendMouseDevice;
            simulateInitialization(&mouseDevice, &backendMouseDevice);

            // THEN
            QCOMPARE(backendMouseDevice.isEnabled(), true);
            QCOMPARE(backendMouseDevice.peerId(), mouseDevice.id());
            QVERIFY(backendMouseDevice.inputHandler() == nullptr);
            QCOMPARE(backendMouseDevice.mouseState().xAxis, 0.0f);
            QCOMPARE(backendMouseDevice.mouseState().yAxis, 0.0f);
            QCOMPARE(backendMouseDevice.mouseState().wXAxis, 0.0f);
            QCOMPARE(backendMouseDevice.mouseState().wYAxis, 0.0f);
            QCOMPARE(backendMouseDevice.mouseState().leftPressed, false);
            QCOMPARE(backendMouseDevice.mouseState().rightPressed, false);
            QCOMPARE(backendMouseDevice.mouseState().centerPressed, false);
            QCOMPARE(backendMouseDevice.previousPos(), QPointF());
            QCOMPARE(backendMouseDevice.wasPressed(), false);
            QCOMPARE(backendMouseDevice.sensitivity(), 0.8f);
        }
        {
            // WHEN
            Qt3DInput::Input::MouseDevice backendMouseDevice;
            mouseDevice.setEnabled(false);
            simulateInitialization(&mouseDevice, &backendMouseDevice);

            // THEN
            QCOMPARE(backendMouseDevice.peerId(), mouseDevice.id());
            QCOMPARE(backendMouseDevice.isEnabled(), false);
        }
    }

    void checkMouseMoveStatesUpdates()
    {
        // GIVEN
        Qt3DInput::Input::MouseDevice backendMouseDevice;

        // WHEN
        auto eventList = QList<QT_PREPEND_NAMESPACE(QMouseEvent)>() << QT_PREPEND_NAMESPACE(QMouseEvent)(QMouseEvent::MouseButtonPress,
                                                                                                         QPointF(400.0f, 400.0f),
                                                                                                         QPointF(400.0f, 400.0f),
                                                                                                         QPointF(400.0f, 400.0f),
                                                                                                         Qt::LeftButton,
                                                                                                         Qt::LeftButton,
                                                                                                         Qt::NoModifier);
        backendMouseDevice.updateMouseEvents(eventList);

        // THEN
        // Note: axis are only modified when moving (> 1 event)
        QCOMPARE(backendMouseDevice.mouseState().xAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().yAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wXAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wYAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().leftPressed, true);
        QCOMPARE(backendMouseDevice.mouseState().rightPressed, false);
        QCOMPARE(backendMouseDevice.mouseState().centerPressed, false);
        QCOMPARE(backendMouseDevice.previousPos(), QPointF(400.0f, 400.0f));
        QCOMPARE(backendMouseDevice.wasPressed(), true);
        QCOMPARE(backendMouseDevice.sensitivity(), 0.1f);

        // WHEN
        eventList = QList<QT_PREPEND_NAMESPACE(QMouseEvent)>() << QT_PREPEND_NAMESPACE(QMouseEvent)(QMouseEvent::MouseMove,
                                                                                                    QPointF(600.0f, 600.0f),
                                                                                                    QPointF(600.0f, 600.0f),
                                                                                                    QPointF(600.0f, 600.0f),
                                                                                                    Qt::LeftButton,
                                                                                                    Qt::LeftButton,
                                                                                                    Qt::NoModifier);
        backendMouseDevice.updateMouseEvents(eventList);

        // THEN
        QCOMPARE(backendMouseDevice.mouseState().xAxis, (600.0f - 400.0f) * 0.1f);
        QCOMPARE(backendMouseDevice.mouseState().yAxis, (400.0f - 600.0f) * 0.1f);
        QCOMPARE(backendMouseDevice.mouseState().wXAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wYAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().leftPressed, true);
        QCOMPARE(backendMouseDevice.mouseState().rightPressed, false);
        QCOMPARE(backendMouseDevice.mouseState().centerPressed, false);
        QCOMPARE(backendMouseDevice.previousPos(), QPointF(600.0f, 600.0f));
        QCOMPARE(backendMouseDevice.wasPressed(), true);
        QCOMPARE(backendMouseDevice.sensitivity(), 0.1f);

        // WHEN
        eventList = QList<QT_PREPEND_NAMESPACE(QMouseEvent)>() << QT_PREPEND_NAMESPACE(QMouseEvent)(QMouseEvent::MouseButtonRelease,
                                                                                                    QPointF(600.0f, 600.0f),
                                                                                                    QPointF(600.0f, 600.0f),
                                                                                                    QPointF(600.0f, 600.0f),
                                                                                                    Qt::LeftButton,
                                                                                                    Qt::NoButton,
                                                                                                    Qt::NoModifier);
        backendMouseDevice.updateMouseEvents(eventList);

        // THEN
        QCOMPARE(backendMouseDevice.mouseState().xAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().yAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wXAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wYAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().leftPressed, false);
        QCOMPARE(backendMouseDevice.mouseState().rightPressed, false);
        QCOMPARE(backendMouseDevice.mouseState().centerPressed, false);
        QCOMPARE(backendMouseDevice.previousPos(), QPointF(600.0f, 600.0f));
        QCOMPARE(backendMouseDevice.wasPressed(), false);
        QCOMPARE(backendMouseDevice.sensitivity(), 0.1f);
    }

    void checkMouseWheelState()
    {
        // GIVEN
        Qt3DInput::Input::MouseDevice backendMouseDevice;

        // WHEN
        auto eventList = QList<QT_PREPEND_NAMESPACE(QWheelEvent)>() << QT_PREPEND_NAMESPACE(QWheelEvent)(QPointF(500.0f, 500.0f),
                                                                                                         120,
                                                                                                         Qt::NoButton,
                                                                                                         Qt::NoModifier,
                                                                                                         Qt::Vertical);
        backendMouseDevice.updateWheelEvents(eventList);

        // THEN
        QCOMPARE(backendMouseDevice.mouseState().wXAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wYAxis, 0.1f * 120);
        QCOMPARE(backendMouseDevice.sensitivity(), 0.1f);

        // WHEN
        eventList = QList<QT_PREPEND_NAMESPACE(QWheelEvent)>() << QT_PREPEND_NAMESPACE(QWheelEvent)(QPointF(500.0f, 500.0f),
                                                                                                    120,
                                                                                                    Qt::NoButton,
                                                                                                    Qt::NoModifier,
                                                                                                    Qt::Horizontal);
        backendMouseDevice.updateWheelEvents(eventList);

        // THEN
        QCOMPARE(backendMouseDevice.mouseState().wXAxis, 0.1f * 120);
        QCOMPARE(backendMouseDevice.mouseState().wYAxis, 0.0f);
        QCOMPARE(backendMouseDevice.sensitivity(), 0.1f);

        // WHEN
        eventList = QList<QT_PREPEND_NAMESPACE(QWheelEvent)>() << QT_PREPEND_NAMESPACE(QWheelEvent)(QPointF(500.0f, 500.0f),
                                                                                                    0,
                                                                                                    Qt::NoButton,
                                                                                                    Qt::NoModifier,
                                                                                                    Qt::Horizontal);
        backendMouseDevice.updateWheelEvents(eventList);

        // THEN
        QCOMPARE(backendMouseDevice.mouseState().wXAxis, 0.0f);
        QCOMPARE(backendMouseDevice.mouseState().wYAxis, 0.0f);
        QCOMPARE(backendMouseDevice.sensitivity(), 0.1f);
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DInput::Input::MouseDevice backendMouseDevice;

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendMouseDevice.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendMouseDevice.isEnabled(), newValue);
        }
        {
            // WHEN
            const float newValue = 99.0f;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("sensitivity");
            change->setValue(QVariant::fromValue(newValue));
            backendMouseDevice.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendMouseDevice.sensitivity(), newValue);
        }
    }

};

QTEST_MAIN(tst_MouseDevice)

#include "tst_mousedevice.moc"
