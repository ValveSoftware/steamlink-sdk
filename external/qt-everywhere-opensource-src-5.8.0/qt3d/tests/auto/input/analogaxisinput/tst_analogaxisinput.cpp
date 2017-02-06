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
#include <qbackendnodetester.h>
#include "testdevice.h"

#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DInput/private/analogaxisinput_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/QAnalogAxisInput>
#include <Qt3DCore/qpropertyupdatedchange.h>

class tst_AnalogAxisInput: public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DInput::Input::AnalogAxisInput backendAxisInput;
        Qt3DInput::QAnalogAxisInput axisInput;
        TestDevice sourceDevice;

        axisInput.setAxis(327);
        axisInput.setSourceDevice(&sourceDevice);

        // WHEN
        simulateInitialization(&axisInput, &backendAxisInput);

        // THEN
        QCOMPARE(backendAxisInput.peerId(), axisInput.id());
        QCOMPARE(backendAxisInput.isEnabled(), axisInput.isEnabled());
        QCOMPARE(backendAxisInput.axis(), axisInput.axis());
        QCOMPARE(backendAxisInput.sourceDevice(), sourceDevice.id());
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DInput::Input::AnalogAxisInput backendAxisInput;

        // THEN
        QVERIFY(backendAxisInput.peerId().isNull());
        QCOMPARE(backendAxisInput.axis(), 0);
        QCOMPARE(backendAxisInput.isEnabled(), false);
        QCOMPARE(backendAxisInput.sourceDevice(), Qt3DCore::QNodeId());

        // GIVEN
        Qt3DInput::QAnalogAxisInput axisInput;
        TestDevice sourceDevice;

        axisInput.setAxis(327);
        axisInput.setSourceDevice(&sourceDevice);

        // WHEN
        simulateInitialization(&axisInput, &backendAxisInput);
        backendAxisInput.cleanup();

        // THEN
        QCOMPARE(backendAxisInput.axis(), 0);
        QCOMPARE(backendAxisInput.isEnabled(), false);
        QCOMPARE(backendAxisInput.sourceDevice(), Qt3DCore::QNodeId());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DInput::Input::AnalogAxisInput backendAxisInput;

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(32);
        updateChange->setPropertyName("axis");
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.axis(), 32);

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
    }

    void shouldProcessAxisValue()
    {
        const qint64 s = 1000000000;

        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        Qt3DInput::Input::AnalogAxisInput backendAxisInput;
        Qt3DInput::QAnalogAxisInput axisInput;
        axisInput.setEnabled(true);
        axisInput.setAxis(2);
        axisInput.setSourceDevice(device);
        simulateInitialization(&axisInput, &backendAxisInput);
        QCOMPARE(backendAxisInput.axis(), 2);

        // WHEN
        deviceBackend->setAxisValue(2, 0.1f);

        // THEN
        QCOMPARE(backendAxisInput.process(&handler, 30 * s), 0.1f);
        QCOMPARE(backendAxisInput.process(&handler, 31 * s), 0.1f);

        // WHEN
        deviceBackend->setAxisValue(2, 0.2f);

        // THEN
        QCOMPARE(backendAxisInput.process(&handler, 32 * s), 0.2f);
        QCOMPARE(backendAxisInput.process(&handler, 33 * s), 0.2f);
    }

    void shouldNotProcessAxisValueWhenDisabled()
    {
        const qint64 s = 1000000000;

        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        Qt3DInput::Input::AnalogAxisInput backendAxisInput;
        Qt3DInput::QAnalogAxisInput axisInput;
        axisInput.setEnabled(false);
        axisInput.setAxis(2);
        axisInput.setSourceDevice(device);
        simulateInitialization(&axisInput, &backendAxisInput);
        QCOMPARE(backendAxisInput.axis(), 2);

        // WHEN
        deviceBackend->setAxisValue(2, 0.1f);

        // THEN
        QCOMPARE(backendAxisInput.process(&handler, 30 * s), 0.0f);
        QCOMPARE(backendAxisInput.process(&handler, 31 * s), 0.0f);

        // WHEN
        deviceBackend->setAxisValue(2, 0.2f);

        // THEN
        QCOMPARE(backendAxisInput.process(&handler, 32 * s), 0.0f);
        QCOMPARE(backendAxisInput.process(&handler, 33 * s), 0.0f);
    }
};

QTEST_APPLESS_MAIN(tst_AnalogAxisInput)

#include "tst_analogaxisinput.moc"
