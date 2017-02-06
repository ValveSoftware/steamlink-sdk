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
#include <qbackendnodetester.h>
#include "testdevice.h"

#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DInput/private/buttonaxisinput_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/QButtonAxisInput>
#include <Qt3DCore/qpropertyupdatedchange.h>

class tst_ButtonAxisInput: public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DInput::Input::ButtonAxisInput backendAxisInput;
        Qt3DInput::QButtonAxisInput axisInput;
        TestDevice sourceDevice;

        axisInput.setButtons(QVector<int>() << (1 << 8));
        axisInput.setScale(0.5f);
        axisInput.setAcceleration(0.42f);
        axisInput.setDeceleration(0.43f);
        axisInput.setSourceDevice(&sourceDevice);

        // WHEN
        simulateInitialization(&axisInput, &backendAxisInput);

        // THEN
        QCOMPARE(backendAxisInput.peerId(), axisInput.id());
        QCOMPARE(backendAxisInput.isEnabled(), axisInput.isEnabled());
        QCOMPARE(backendAxisInput.buttons(), axisInput.buttons());
        QCOMPARE(backendAxisInput.scale(), axisInput.scale());
        QCOMPARE(backendAxisInput.acceleration(), axisInput.acceleration());
        QCOMPARE(backendAxisInput.deceleration(), axisInput.deceleration());
        QCOMPARE(backendAxisInput.sourceDevice(), sourceDevice.id());

        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 0);
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DInput::Input::ButtonAxisInput backendAxisInput;

        // THEN
        QVERIFY(backendAxisInput.peerId().isNull());
        QCOMPARE(backendAxisInput.scale(), 0.0f);
        QVERIFY(qIsInf(backendAxisInput.acceleration()));
        QVERIFY(qIsInf(backendAxisInput.deceleration()));
        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 0);
        QVERIFY(backendAxisInput.buttons().isEmpty());
        QCOMPARE(backendAxisInput.isEnabled(), false);
        QCOMPARE(backendAxisInput.sourceDevice(), Qt3DCore::QNodeId());

        // GIVEN
        Qt3DInput::QButtonAxisInput axisInput;
        TestDevice sourceDevice;

        axisInput.setButtons(QVector<int>() << (1 << 8));
        axisInput.setScale(0.5f);
        axisInput.setSourceDevice(&sourceDevice);

        // WHEN
        simulateInitialization(&axisInput, &backendAxisInput);
        backendAxisInput.cleanup();

        // THEN
        QCOMPARE(backendAxisInput.scale(), 0.0f);
        QVERIFY(qIsInf(backendAxisInput.acceleration()));
        QVERIFY(qIsInf(backendAxisInput.deceleration()));
        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 0);
        QVERIFY(backendAxisInput.buttons().isEmpty());
        QCOMPARE(backendAxisInput.isEnabled(), false);
        QCOMPARE(backendAxisInput.sourceDevice(), Qt3DCore::QNodeId());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DInput::Input::ButtonAxisInput backendAxisInput;

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr  updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(QVariant::fromValue(QVector<int>() << 64));
        updateChange->setPropertyName("buttons");
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.buttons(), QVector<int>() << 64);

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(0.5f);
        updateChange->setPropertyName("scale");
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.scale(), 0.5f);

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.isEnabled(), true);

        // WHEN
        TestDevice device;
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("sourceDevice");
        updateChange->setValue(QVariant::fromValue(device.id()));
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.sourceDevice(), device.id());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(0.42f);
        updateChange->setPropertyName("acceleration");
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.acceleration(), 0.42f);

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(-0.42f);
        updateChange->setPropertyName("acceleration");
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QVERIFY(qIsInf(backendAxisInput.acceleration()));

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(0.43f);
        updateChange->setPropertyName("deceleration");
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.deceleration(), 0.43f);

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(-0.43f);
        updateChange->setPropertyName("deceleration");
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QVERIFY(qIsInf(backendAxisInput.deceleration()));
    }

    void shouldProcessAndUpdateSpeedRatioOverTime()
    {
        const qint64 s = 1000000000;

        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        Qt3DInput::Input::ButtonAxisInput backendAxisInput;
        Qt3DInput::QButtonAxisInput axisInput;
        axisInput.setEnabled(true);
        axisInput.setButtons(QVector<int>() << Qt::Key_Space);
        axisInput.setScale(-1.0f);
        axisInput.setAcceleration(0.15f);
        axisInput.setDeceleration(0.3f);
        axisInput.setSourceDevice(device);
        simulateInitialization(&axisInput, &backendAxisInput);
        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 0);

        // WHEN (accelerate)
        deviceBackend->setButtonPressed(Qt::Key_Space, true);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 30 * s), 0.0f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 30 * s);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 31 * s), -0.15f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.15f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 31 * s);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 32 * s), -0.3f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.3f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 32 * s);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 35 * s), -0.75f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.75f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 35 * s);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 37 * s), -1.0f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 1.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 37 * s);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 38 * s), -1.0f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 1.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 38 * s);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 42 * s), -1.0f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 1.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 42 * s);


        // WHEN (decelerate)
        deviceBackend->setButtonPressed(Qt::Key_Space, false);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 43 * s), -0.7f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.7f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 43 * s);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 45 * s), -0.1f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.1f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 45 * s);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 46 * s), 0.0f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 0);
    }

    void shouldNotProcessWhenDisabled()
    {
        const qint64 s = 1000000000;

        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        Qt3DInput::Input::ButtonAxisInput backendAxisInput;
        Qt3DInput::QButtonAxisInput axisInput;
        axisInput.setEnabled(false);
        axisInput.setButtons(QVector<int>() << Qt::Key_Space);
        axisInput.setScale(-1.0f);
        axisInput.setAcceleration(0.15f);
        axisInput.setDeceleration(0.3f);
        axisInput.setSourceDevice(device);
        simulateInitialization(&axisInput, &backendAxisInput);
        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 0);

        // WHEN (accelerate)
        deviceBackend->setButtonPressed(Qt::Key_Space, true);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 30 * s), 0.0f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 0);

        // WHEN
        QCOMPARE(backendAxisInput.process(&handler, 31 * s), 0.0f);

        // THEN
        QCOMPARE(backendAxisInput.speedRatio(), 0.0f);
        QCOMPARE(backendAxisInput.lastUpdateTime(), 0);
    }
};

QTEST_APPLESS_MAIN(tst_ButtonAxisInput)

#include "tst_buttonaxisinput.moc"
