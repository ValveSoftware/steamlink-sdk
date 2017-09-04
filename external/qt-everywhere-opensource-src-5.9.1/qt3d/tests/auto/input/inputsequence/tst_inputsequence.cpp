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

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>
#include <Qt3DInput/private/actioninput_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/private/inputsequence_p.h>
#include <Qt3DInput/QActionInput>
#include <Qt3DInput/QInputSequence>

class tst_InputSequence : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:
    void shouldMirrorPeerProperties()
    {
        // GIVEN
        Qt3DInput::Input::InputSequence backendInputSequence;
        Qt3DInput::QInputSequence inputSequence;
        Qt3DInput::QActionInput actionInput;

        inputSequence.setTimeout(250);
        inputSequence.setButtonInterval(100);
        inputSequence.addSequence(&actionInput);

        // WHEN
        simulateInitialization(&inputSequence, &backendInputSequence);

        // THEN
        QCOMPARE(backendInputSequence.peerId(), inputSequence.id());
        QCOMPARE(backendInputSequence.isEnabled(), inputSequence.isEnabled());
        QCOMPARE(backendInputSequence.timeout(), inputSequence.timeout() * 1000000);
        QCOMPARE(backendInputSequence.buttonInterval(), inputSequence.buttonInterval() * 1000000);
        QCOMPARE(backendInputSequence.sequences().size(), inputSequence.sequences().size());

        const int inputsCount = backendInputSequence.sequences().size();
        if (inputsCount > 0) {
            for (int i = 0; i < inputsCount; ++i)
                QCOMPARE(backendInputSequence.sequences().at(i), inputSequence.sequences().at(i)->id());
        }
    }

    void shouldHaveInitialAndCleanedUpStates()
    {
        // GIVEN
        Qt3DInput::Input::InputSequence backendInputSequence;

        // THEN
        QVERIFY(backendInputSequence.peerId().isNull());
        QCOMPARE(backendInputSequence.isEnabled(), false);
        QCOMPARE(backendInputSequence.timeout(), 0);
        QCOMPARE(backendInputSequence.buttonInterval(), 0);
        QCOMPARE(backendInputSequence.sequences().size(), 0);

        // GIVEN
        Qt3DInput::QInputSequence inputSequence;
        Qt3DInput::QActionInput actionInput;

        inputSequence.setTimeout(250);
        inputSequence.setButtonInterval(100);
        inputSequence.addSequence(&actionInput);

        // WHEN
        simulateInitialization(&inputSequence, &backendInputSequence);
        backendInputSequence.cleanup();

        // THEN
        QCOMPARE(backendInputSequence.isEnabled(), false);
        QCOMPARE(backendInputSequence.timeout(), 0);
        QCOMPARE(backendInputSequence.buttonInterval(), 0);
        QCOMPARE(backendInputSequence.sequences().size(), 0);
    }

    void shouldHandlePropertyChanges()
    {
        // GIVEN
        Qt3DInput::Input::InputSequence backendInputSequence;

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(250);
        updateChange->setPropertyName("timeout");
        backendInputSequence.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendInputSequence.timeout(), 250000000);

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(150);
        updateChange->setPropertyName("buttonInterval");
        backendInputSequence.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendInputSequence.buttonInterval(), 150000000);

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendInputSequence.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendInputSequence.isEnabled(), true);

        // WHEN
        Qt3DInput::QActionInput input;
        const Qt3DCore::QNodeId inputId = input.id();
        const auto nodeAddedChange = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &input);
        nodeAddedChange->setPropertyName("sequence");
        backendInputSequence.sceneChangeEvent(nodeAddedChange);

        // THEN
        QCOMPARE(backendInputSequence.sequences().size(), 1);
        QCOMPARE(backendInputSequence.sequences().first(), inputId);

        // WHEN
        const auto nodeRemovedChange = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &input);
        nodeRemovedChange->setPropertyName("sequence");
        backendInputSequence.sceneChangeEvent(nodeRemovedChange);

        // THEN
        QCOMPARE(backendInputSequence.sequences().size(), 0);
    }

    void shouldActivateWhenSequenceIsConsumedInOrderOnly()
    {
        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        auto firstInput = new Qt3DInput::QActionInput;
        firstInput->setButtons(QVector<int>() << Qt::Key_Q << Qt::Key_A);
        firstInput->setSourceDevice(device);
        auto backendFirstInput = handler.actionInputManager()->getOrCreateResource(firstInput->id());
        simulateInitialization(firstInput, backendFirstInput);

        auto secondInput = new Qt3DInput::QActionInput;
        secondInput->setButtons(QVector<int>() << Qt::Key_S << Qt::Key_W);
        secondInput->setSourceDevice(device);
        auto backendSecondInput = handler.actionInputManager()->getOrCreateResource(secondInput->id());
        simulateInitialization(secondInput, backendSecondInput);

        auto thirdInput = new Qt3DInput::QActionInput;
        thirdInput->setButtons(QVector<int>() << Qt::Key_D << Qt::Key_E);
        thirdInput->setSourceDevice(device);
        auto backendThirdInput = handler.actionInputManager()->getOrCreateResource(thirdInput->id());
        simulateInitialization(thirdInput, backendThirdInput);

        Qt3DInput::Input::InputSequence backendInputSequence;
        Qt3DInput::QInputSequence inputSequence;
        inputSequence.setEnabled(true);
        inputSequence.setButtonInterval(150);
        inputSequence.setTimeout(450);
        inputSequence.addSequence(firstInput);
        inputSequence.addSequence(secondInput);
        inputSequence.addSequence(thirdInput);
        simulateInitialization(&inputSequence, &backendInputSequence);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Up, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1000000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Up, false);
        deviceBackend->setButtonPressed(Qt::Key_Q, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1100000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, false);
        deviceBackend->setButtonPressed(Qt::Key_S, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1200000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_S, false);
        deviceBackend->setButtonPressed(Qt::Key_E, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1300000000), true);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_E, false);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1400000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1500000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, false);
        deviceBackend->setButtonPressed(Qt::Key_S, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1600000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_S, false);
        deviceBackend->setButtonPressed(Qt::Key_E, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1700000000), true);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_E, false);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1800000000), false);


        // Now out of order

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_S, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1900000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_S, false);
        deviceBackend->setButtonPressed(Qt::Key_Q, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 2000000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, false);
        deviceBackend->setButtonPressed(Qt::Key_D, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 2100000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_D, false);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 22000000000), false);
    }

    void shouldRespectSequenceTimeout()
    {
        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        auto firstInput = new Qt3DInput::QActionInput;
        firstInput->setButtons(QVector<int>() << Qt::Key_Q << Qt::Key_A);
        firstInput->setSourceDevice(device);
        auto backendFirstInput = handler.actionInputManager()->getOrCreateResource(firstInput->id());
        simulateInitialization(firstInput, backendFirstInput);

        auto secondInput = new Qt3DInput::QActionInput;
        secondInput->setButtons(QVector<int>() << Qt::Key_S << Qt::Key_W);
        secondInput->setSourceDevice(device);
        auto backendSecondInput = handler.actionInputManager()->getOrCreateResource(secondInput->id());
        simulateInitialization(secondInput, backendSecondInput);

        auto thirdInput = new Qt3DInput::QActionInput;
        thirdInput->setButtons(QVector<int>() << Qt::Key_D << Qt::Key_E);
        thirdInput->setSourceDevice(device);
        auto backendThirdInput = handler.actionInputManager()->getOrCreateResource(thirdInput->id());
        simulateInitialization(thirdInput, backendThirdInput);

        Qt3DInput::Input::InputSequence backendInputSequence;
        Qt3DInput::QInputSequence inputSequence;
        inputSequence.setEnabled(true);
        inputSequence.setButtonInterval(250);
        inputSequence.setTimeout(450);
        inputSequence.addSequence(firstInput);
        inputSequence.addSequence(secondInput);
        inputSequence.addSequence(thirdInput);
        simulateInitialization(&inputSequence, &backendInputSequence);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1100000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, false);
        deviceBackend->setButtonPressed(Qt::Key_S, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1300000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_S, false);
        deviceBackend->setButtonPressed(Qt::Key_E, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1600000000), false); // Too late
    }

    void shouldRespectSequenceButtonInterval()
    {
        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        auto firstInput = new Qt3DInput::QActionInput;
        firstInput->setButtons(QVector<int>() << Qt::Key_Q << Qt::Key_A);
        firstInput->setSourceDevice(device);
        auto backendFirstInput = handler.actionInputManager()->getOrCreateResource(firstInput->id());
        simulateInitialization(firstInput, backendFirstInput);

        auto secondInput = new Qt3DInput::QActionInput;
        secondInput->setButtons(QVector<int>() << Qt::Key_S << Qt::Key_W);
        secondInput->setSourceDevice(device);
        auto backendSecondInput = handler.actionInputManager()->getOrCreateResource(secondInput->id());
        simulateInitialization(secondInput, backendSecondInput);

        auto thirdInput = new Qt3DInput::QActionInput;
        thirdInput->setButtons(QVector<int>() << Qt::Key_D << Qt::Key_E);
        thirdInput->setSourceDevice(device);
        auto backendThirdInput = handler.actionInputManager()->getOrCreateResource(thirdInput->id());
        simulateInitialization(thirdInput, backendThirdInput);

        Qt3DInput::Input::InputSequence backendInputSequence;
        Qt3DInput::QInputSequence inputSequence;
        inputSequence.setEnabled(true);
        inputSequence.setButtonInterval(100);
        inputSequence.setTimeout(450);
        inputSequence.addSequence(firstInput);
        inputSequence.addSequence(secondInput);
        inputSequence.addSequence(thirdInput);
        simulateInitialization(&inputSequence, &backendInputSequence);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1100000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, false);
        deviceBackend->setButtonPressed(Qt::Key_S, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1250000000), false); // Too late

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_S, false);
        deviceBackend->setButtonPressed(Qt::Key_E, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1300000000), false);
    }

    void shouldNotProcessWhenDisabled()
    {
        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        auto firstInput = new Qt3DInput::QActionInput;
        firstInput->setButtons(QVector<int>() << Qt::Key_Q);
        firstInput->setSourceDevice(device);
        auto backendFirstInput = handler.actionInputManager()->getOrCreateResource(firstInput->id());
        simulateInitialization(firstInput, backendFirstInput);

        auto secondInput = new Qt3DInput::QActionInput;
        secondInput->setButtons(QVector<int>() << Qt::Key_S);
        secondInput->setSourceDevice(device);
        auto backendSecondInput = handler.actionInputManager()->getOrCreateResource(secondInput->id());
        simulateInitialization(secondInput, backendSecondInput);

        Qt3DInput::Input::InputSequence backendInputSequence;
        Qt3DInput::QInputSequence inputSequence;
        inputSequence.setEnabled(false);
        inputSequence.setButtonInterval(150);
        inputSequence.setTimeout(450);
        inputSequence.addSequence(firstInput);
        inputSequence.addSequence(secondInput);
        simulateInitialization(&inputSequence, &backendInputSequence);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1000000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, false);
        deviceBackend->setButtonPressed(Qt::Key_S, true);

        // THEN
        QCOMPARE(backendInputSequence.process(&handler, 1100000000), false);
    }
};

QTEST_APPLESS_MAIN(tst_InputSequence)

#include "tst_inputsequence.moc"
