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
#include <Qt3DInput/private/inputchord_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/QActionInput>
#include <Qt3DInput/QInputChord>

class tst_InputChord : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:
    void shouldMirrorPeerProperties()
    {
        // GIVEN
        Qt3DInput::Input::InputChord backendInputChord;
        Qt3DInput::QInputChord inputChord;
        Qt3DInput::QActionInput actionInput;

        inputChord.setTimeout(250);
        inputChord.addChord(&actionInput);

        // WHEN
        simulateInitialization(&inputChord, &backendInputChord);

        // THEN
        QCOMPARE(backendInputChord.peerId(), inputChord.id());
        QCOMPARE(backendInputChord.isEnabled(), inputChord.isEnabled());
        QCOMPARE(backendInputChord.timeout(), inputChord.timeout() * 1000000);
        QCOMPARE(backendInputChord.chords().size(), inputChord.chords().size());

        const int inputsCount = backendInputChord.chords().size();
        if (inputsCount > 0) {
            for (int i = 0; i < inputsCount; ++i)
                QCOMPARE(backendInputChord.chords().at(i), inputChord.chords().at(i)->id());
        }
    }

    void shouldHaveInitialAndCleanedUpStates()
    {
        // GIVEN
        Qt3DInput::Input::InputChord backendInputChord;

        // THEN
        QVERIFY(backendInputChord.peerId().isNull());
        QCOMPARE(backendInputChord.isEnabled(), false);
        QCOMPARE(backendInputChord.timeout(), 0);
        QCOMPARE(backendInputChord.chords().size(), 0);

        // GIVEN
        Qt3DInput::QInputChord inputChord;
        Qt3DInput::QActionInput actionInput;

        inputChord.setTimeout(250);
        inputChord.addChord(&actionInput);

        // WHEN
        simulateInitialization(&inputChord, &backendInputChord);
        backendInputChord.cleanup();

        // THEN
        QCOMPARE(backendInputChord.isEnabled(), false);
        QCOMPARE(backendInputChord.timeout(), 0);
        QCOMPARE(backendInputChord.chords().size(), 0);
    }

    void shouldHandlePropertyChanges()
    {
        // GIVEN
        Qt3DInput::Input::InputChord backendInputChord;

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(250);
        updateChange->setPropertyName("timeout");
        backendInputChord.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendInputChord.timeout(), 250000000);

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendInputChord.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendInputChord.isEnabled(), true);

        // WHEN
        Qt3DInput::QActionInput input;
        const Qt3DCore::QNodeId inputId = input.id();
        const auto nodeAddedChange = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &input);
        nodeAddedChange->setPropertyName("chord");
        backendInputChord.sceneChangeEvent(nodeAddedChange);

        // THEN
        QCOMPARE(backendInputChord.chords().size(), 1);
        QCOMPARE(backendInputChord.chords().first(), inputId);

        // WHEN
        const auto nodeRemovedChange = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &input);
        nodeRemovedChange->setPropertyName("chord");
        backendInputChord.sceneChangeEvent(nodeRemovedChange);

        // THEN
        QCOMPARE(backendInputChord.chords().size(), 0);
    }

    void shouldActivateWhenAllArePressed()
    {
        // GIVEN
        TestDeviceIntegration deviceIntegration;
        TestDevice *device = deviceIntegration.createPhysicalDevice("keyboard");
        TestDeviceBackendNode *deviceBackend = deviceIntegration.physicalDevice(device->id());
        Qt3DInput::Input::InputHandler handler;
        handler.addInputDeviceIntegration(&deviceIntegration);

        auto firstInput = new Qt3DInput::QActionInput;
        firstInput->setButtons(QVector<int>() << Qt::Key_Q << Qt::Key_W);
        firstInput->setSourceDevice(device);
        auto backendFirstInput = handler.actionInputManager()->getOrCreateResource(firstInput->id());
        simulateInitialization(firstInput, backendFirstInput);

        auto secondInput = new Qt3DInput::QActionInput;
        secondInput->setButtons(QVector<int>() << Qt::Key_A << Qt::Key_S);
        secondInput->setSourceDevice(device);
        auto backendSecondInput = handler.actionInputManager()->getOrCreateResource(secondInput->id());
        simulateInitialization(secondInput, backendSecondInput);

        Qt3DInput::Input::InputChord backendInputChord;
        Qt3DInput::QInputChord inputChord;
        inputChord.setEnabled(true);
        inputChord.setTimeout(300);
        inputChord.addChord(firstInput);
        inputChord.addChord(secondInput);
        simulateInitialization(&inputChord, &backendInputChord);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Up, true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1000000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Up, false);
        deviceBackend->setButtonPressed(Qt::Key_Q, true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1100000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_A, true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1200000000), true);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_S, true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1300000000), true);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_W, true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1400000000), true);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_W, false);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1500000000), true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1600000000), true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1700000000), true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1800000000), true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1900000000), true);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, false);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 2000000000), false);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 2100000000), false);
    }

    void shouldRespectChordTimeout()
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
        secondInput->setButtons(QVector<int>() << Qt::Key_W);
        secondInput->setSourceDevice(device);
        auto backendSecondInput = handler.actionInputManager()->getOrCreateResource(secondInput->id());
        simulateInitialization(secondInput, backendSecondInput);

        Qt3DInput::Input::InputChord backendInputChord;
        Qt3DInput::QInputChord inputChord;
        inputChord.setEnabled(true);
        inputChord.setTimeout(300);
        inputChord.addChord(firstInput);
        inputChord.addChord(secondInput);
        simulateInitialization(&inputChord, &backendInputChord);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1000000000), false);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_W, true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1400000000), false); // Too late

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1600000000), false);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1800000000), false);
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
        secondInput->setButtons(QVector<int>() << Qt::Key_W);
        secondInput->setSourceDevice(device);
        auto backendSecondInput = handler.actionInputManager()->getOrCreateResource(secondInput->id());
        simulateInitialization(secondInput, backendSecondInput);

        Qt3DInput::Input::InputChord backendInputChord;
        Qt3DInput::QInputChord inputChord;
        inputChord.setEnabled(false);
        inputChord.setTimeout(300);
        inputChord.addChord(firstInput);
        inputChord.addChord(secondInput);
        simulateInitialization(&inputChord, &backendInputChord);

        // WHEN
        deviceBackend->setButtonPressed(Qt::Key_Q, true);
        deviceBackend->setButtonPressed(Qt::Key_W, true);

        // THEN
        QCOMPARE(backendInputChord.process(&handler, 1000000000), false);
    }
};

QTEST_APPLESS_MAIN(tst_InputChord)

#include "tst_inputchord.moc"
