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

#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/QKeyboardDevice>
#include <Qt3DInput/private/keyboarddevice_p.h>
#include <Qt3DInput/QKeyboardHandler>
#include <Qt3DInput/private/keyboardhandler_p.h>

class tst_KeyboardHandler : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DInput::Input::InputHandler inputHandler;

        auto keyboardDevice = new Qt3DInput::QKeyboardDevice;
        auto backendKeyboardDevice = inputHandler.keyboardDeviceManager()->getOrCreateResource(keyboardDevice->id());
        backendKeyboardDevice->setInputHandler(&inputHandler);
        simulateInitialization(keyboardDevice, backendKeyboardDevice);

        Qt3DInput::QKeyboardHandler keyboardHandler;
        auto backendKeyboardHandler = inputHandler.keyboardInputManager()->getOrCreateResource(keyboardHandler.id());
        backendKeyboardHandler->setInputHandler(&inputHandler);

        keyboardHandler.setFocus(true);
        keyboardHandler.setSourceDevice(keyboardDevice);

        // WHEN
        simulateInitialization(&keyboardHandler, backendKeyboardHandler);

        // THEN
        QCOMPARE(backendKeyboardHandler->peerId(), keyboardHandler.id());
        QCOMPARE(backendKeyboardHandler->isEnabled(), keyboardHandler.isEnabled());
        QCOMPARE(backendKeyboardDevice->lastKeyboardInputRequester(), backendKeyboardHandler->peerId());
        QCOMPARE(backendKeyboardHandler->keyboardDevice(), keyboardDevice->id());
    }

    void checkInitialState()
    {
        // GIVEN
        Qt3DInput::Input::KeyboardHandler backendKeyboardHandler;

        // THEN
        QVERIFY(backendKeyboardHandler.peerId().isNull());
        QCOMPARE(backendKeyboardHandler.isEnabled(), false);
        QCOMPARE(backendKeyboardHandler.focus(), false);
        QCOMPARE(backendKeyboardHandler.keyboardDevice(), Qt3DCore::QNodeId());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DInput::Input::InputHandler inputHandler;

        Qt3DInput::QKeyboardDevice device;
        Qt3DInput::Input::KeyboardDevice *backendKeyboardDevice =
            inputHandler.keyboardDeviceManager()->getOrCreateResource(device.id());
        backendKeyboardDevice->setInputHandler(&inputHandler);

        Qt3DInput::Input::KeyboardHandler *backendKeyboardHandler
                = inputHandler.keyboardInputManager()->getOrCreateResource(Qt3DCore::QNodeId::createId());
        backendKeyboardHandler->setInputHandler(&inputHandler);

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("sourceDevice");
        updateChange->setValue(QVariant::fromValue(device.id()));
        backendKeyboardHandler->sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendKeyboardHandler->keyboardDevice(), device.id());

        // WHEN (still disabled, nothing should happen)
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("focus");
        updateChange->setValue(true);
        backendKeyboardHandler->sceneChangeEvent(updateChange);

        // THEN
        QVERIFY(backendKeyboardDevice->lastKeyboardInputRequester().isNull());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendKeyboardHandler->sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendKeyboardHandler->isEnabled(), true);

        // WHEN (now enabled, should request focus)
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("focus");
        updateChange->setValue(true);
        backendKeyboardHandler->sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendKeyboardDevice->lastKeyboardInputRequester(), backendKeyboardHandler->peerId());
    }
};

QTEST_APPLESS_MAIN(tst_KeyboardHandler)

#include "tst_keyboardhandler.moc"
