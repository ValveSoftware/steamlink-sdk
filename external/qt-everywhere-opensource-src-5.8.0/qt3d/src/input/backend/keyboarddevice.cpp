/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "keyboarddevice_p.h"
#include "inputhandler_p.h"
#include "inputmanagers_p.h"
#include <Qt3DCore/qnode.h>

#include <QKeyEvent>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {
namespace Input {

namespace {

QPair<int, int> getKeyPosition(int key)
{
    QPair<int, int> position;

    switch (key) {
    case Qt::Key_Escape:
        return QPair<int, int>(0, 0);
    case Qt::Key_Tab:
        return QPair<int, int>(0, 1);
    case Qt::Key_Backtab:
        return QPair<int, int>(0, 2);
    case Qt::Key_Backspace:
        return QPair<int, int>(0, 3);
    case Qt::Key_Return:
        return QPair<int, int>(0, 4);
    case Qt::Key_Enter:
        return QPair<int, int>(0, 5);
    case Qt::Key_Insert:
        return QPair<int, int>(0, 6);
    case Qt::Key_Delete:
        return QPair<int, int>(0, 7);
    case Qt::Key_Pause:
        return QPair<int, int>(0, 8);
    case Qt::Key_Print:
        return QPair<int, int>(0, 9);
    case Qt::Key_SysReq:
        return QPair<int, int>(0, 10);
    case Qt::Key_Clear:
        return QPair<int, int>(0, 11);
    case Qt::Key_Home:
        return QPair<int, int>(0, 12);
    case Qt::Key_End:
        return QPair<int, int>(0, 13);
    case Qt::Key_Left:
        return QPair<int, int>(0, 14);
    case Qt::Key_Right:
        return QPair<int, int>(0, 16);
    case Qt::Key_Up:
        return QPair<int, int>(0, 15);
    case Qt::Key_Down:
        return QPair<int, int>(0, 17);
    case Qt::Key_PageUp:
        return QPair<int, int>(0, 18);
    case Qt::Key_PageDown:
        return QPair<int, int>(0, 19);
    case Qt::Key_Shift:
        return QPair<int, int>(0, 20);
    case Qt::Key_Alt:
        return QPair<int, int>(0, 23);
    case Qt::Key_Control:
        return QPair<int, int>(0, 21);
    case Qt::Key_Meta:
        return QPair<int, int>(0, 22);
    case Qt::Key_CapsLock:
        return QPair<int, int>(0, 24);
    case Qt::Key_NumLock:
        return QPair<int, int>(0, 25);
    case Qt::Key_ScrollLock:
        return QPair<int, int>(0, 26);
    case Qt::Key_F1:
        return QPair<int, int>(0, 27);
    case Qt::Key_F2:
        return QPair<int, int>(0, 28);
    case Qt::Key_F3:
        return QPair<int, int>(0, 29);
    case Qt::Key_F4:
        return QPair<int, int>(0, 30);
    case Qt::Key_F5:
        return QPair<int, int>(0, 31);
    case Qt::Key_F6:
        return QPair<int, int>(1, 0);
    case Qt::Key_F7:
        return QPair<int, int>(1, 1);
    case Qt::Key_F8:
        return QPair<int, int>(1, 2);
    case Qt::Key_F9:
        return QPair<int, int>(1, 3);
    case Qt::Key_F10:
        return QPair<int, int>(1, 4);
    case Qt::Key_F11:
        return QPair<int, int>(1, 5);
    case Qt::Key_F12:
        return QPair<int, int>(1, 6);
    case Qt::Key_F13:
        return QPair<int, int>(1, 7);
    case Qt::Key_F14:
        return QPair<int, int>(1, 8);
    case Qt::Key_F15:
        return QPair<int, int>(1, 9);
    case Qt::Key_F16:
        return QPair<int, int>(1, 10);
    case Qt::Key_F17:
        return QPair<int, int>(1, 11);
    case Qt::Key_F18:
        return QPair<int, int>(1, 12);
    case Qt::Key_F19:
        return QPair<int, int>(1, 13);
    case Qt::Key_F20:
        return QPair<int, int>(1, 14);
    case Qt::Key_F21:
        return QPair<int, int>(1, 15);
    case Qt::Key_F22:
        return QPair<int, int>(1, 16);
    case Qt::Key_F23:
        return QPair<int, int>(1, 17);
    case Qt::Key_F24:
        return QPair<int, int>(1, 18);
    case Qt::Key_F25:
        return QPair<int, int>(1, 19);
    case Qt::Key_F26:
        return QPair<int, int>(1, 20);
    case Qt::Key_F27:
        return QPair<int, int>(1, 21);
    case Qt::Key_F28:
        return QPair<int, int>(1, 22);
    case Qt::Key_F29:
        return QPair<int, int>(1, 23);
    case Qt::Key_F30:
        return QPair<int, int>(1, 24);
    case Qt::Key_F31:
        return QPair<int, int>(1, 25);
    case Qt::Key_F32:
        return QPair<int, int>(1, 26);
    case Qt::Key_F33:
        return QPair<int, int>(1, 27);
    case Qt::Key_F34:
        return QPair<int, int>(1, 28);
    case Qt::Key_F35:
        return QPair<int, int>(1, 29);
    case Qt::Key_Super_L:
        return QPair<int, int>(1, 30);
    case Qt::Key_Super_R:
        return QPair<int, int>(1, 31);
    case Qt::Key_Menu:
        return QPair<int, int>(2, 1);
    case Qt::Key_Help:
        return QPair<int, int>(2, 4);
    case Qt::Key_Hyper_L:
        return QPair<int, int>(2, 2);
    case Qt::Key_Hyper_R:
        return QPair<int, int>(2, 3);
    case Qt::Key_Direction_L:
        return QPair<int, int>(2, 5);
    case Qt::Key_Direction_R:
        return QPair<int, int>(2, 6);
    case Qt::Key_Space:
        return QPair<int, int>(2, 7);
    case Qt::Key_Exclam:
        return QPair<int, int>(2, 9);
    case Qt::Key_QuoteDbl:
        return QPair<int, int>(2, 10);
    case Qt::Key_NumberSign:
        return QPair<int, int>(2, 11);
    case Qt::Key_Dollar:
        return QPair<int, int>(2, 12);
    case Qt::Key_Percent:
        return QPair<int, int>(2, 13);
    case Qt::Key_Ampersand:
        return QPair<int, int>(2, 14);
    case Qt::Key_Apostrophe:
        return QPair<int, int>(2, 15);
    case Qt::Key_ParenLeft:
        return QPair<int, int>(2, 16);
    case Qt::Key_ParenRight:
        return QPair<int, int>(2, 17);
    case Qt::Key_Asterisk:
        return QPair<int, int>(2, 18);
    case Qt::Key_Plus:
        return QPair<int, int>(2, 19);
    case Qt::Key_Comma:
        return QPair<int, int>(2, 20);
    case Qt::Key_Minus:
        return QPair<int, int>(2, 21);
    case Qt::Key_Period:
        return QPair<int, int>(2, 22);
    case Qt::Key_Slash:
        return QPair<int, int>(2, 23);
    case Qt::Key_0:
        return QPair<int, int>(2, 24);
    case Qt::Key_1:
        return QPair<int, int>(2, 25);
    case Qt::Key_2:
        return QPair<int, int>(2, 26);
    case Qt::Key_3:
        return QPair<int, int>(2, 27);
    case Qt::Key_4:
        return QPair<int, int>(2, 28);
    case Qt::Key_5:
        return QPair<int, int>(2, 29);
    case Qt::Key_6:
        return QPair<int, int>(2, 30);
    case Qt::Key_7:
        return QPair<int, int>(2, 31);
    case Qt::Key_8:
        return QPair<int, int>(3, 0);
    case Qt::Key_9:
        return QPair<int, int>(3, 1);
    case Qt::Key_Colon:
        return QPair<int, int>(3, 2);
    case Qt::Key_Semicolon:
        return QPair<int, int>(3, 3);
    case Qt::Key_Less:
        return QPair<int, int>(3, 4);
    case Qt::Key_Equal:
        return QPair<int, int>(3, 5);
    case Qt::Key_Greater:
        return QPair<int, int>(3, 6);
    case Qt::Key_Question:
        return QPair<int, int>(3, 7);
    case Qt::Key_At:
        return QPair<int, int>(3, 8);
    case Qt::Key_A:
        return QPair<int, int>(3, 9);
    case Qt::Key_B:
        return QPair<int, int>(3, 10);
    case Qt::Key_C:
        return QPair<int, int>(3, 11);
    case Qt::Key_D:
        return QPair<int, int>(3, 12);
    case Qt::Key_E:
        return QPair<int, int>(3, 13);
    case Qt::Key_F:
        return QPair<int, int>(3, 14);
    case Qt::Key_G:
        return QPair<int, int>(3, 15);
    case Qt::Key_H:
        return QPair<int, int>(3, 16);
    case Qt::Key_I:
        return QPair<int, int>(3, 17);
    case Qt::Key_J:
        return QPair<int, int>(3, 18);
    case Qt::Key_K:
        return QPair<int, int>(3, 19);
    case Qt::Key_L:
        return QPair<int, int>(3, 20);
    case Qt::Key_M:
        return QPair<int, int>(3, 21);
    case Qt::Key_N:
        return QPair<int, int>(3, 22);
    case Qt::Key_O:
        return QPair<int, int>(3, 23);
    case Qt::Key_P:
        return QPair<int, int>(3, 24);
    case Qt::Key_Q:
        return QPair<int, int>(3, 25);
    case Qt::Key_R:
        return QPair<int, int>(3, 26);
    case Qt::Key_S:
        return QPair<int, int>(3, 27);
    case Qt::Key_T:
        return QPair<int, int>(3, 28);
    case Qt::Key_U:
        return QPair<int, int>(3, 29);
    case Qt::Key_V:
        return QPair<int, int>(3, 30);
    case Qt::Key_W:
        return QPair<int, int>(3, 31);
    case Qt::Key_X:
        return QPair<int, int>(4, 0);
    case Qt::Key_Y:
        return QPair<int, int>(4, 1);
    case Qt::Key_Z:
        return QPair<int, int>(4, 2);
    case Qt::Key_BracketLeft:
        return QPair<int, int>(4, 3);
    case Qt::Key_Backslash:
        return QPair<int, int>(4, 4);
    case Qt::Key_BracketRight:
        return QPair<int, int>(4, 5);
    case Qt::Key_AsciiCircum:
        return QPair<int, int>(4, 6);
    case Qt::Key_Underscore:
        return QPair<int, int>(4, 7);
    case Qt::Key_QuoteLeft:
        return QPair<int, int>(4, 8);
    case Qt::Key_BraceLeft:
        return QPair<int, int>(4, 9);
    case Qt::Key_Bar:
        return QPair<int, int>(4, 10);
    case Qt::Key_BraceRight:
        return QPair<int, int>(4, 11);
    case Qt::Key_AsciiTilde:
        return QPair<int, int>(4, 12);
    case Qt::Key_plusminus:
        return QPair<int, int>(4, 13);
    case Qt::Key_onesuperior:
        return QPair<int, int>(4, 14);
    case Qt::Key_multiply:
        return QPair<int, int>(4, 15);
    case Qt::Key_division:
        return QPair<int, int>(4, 16);
    case Qt::Key_diaeresis:
        return QPair<int, int>(4, 17);
    default:
        return QPair<int, int>(-1, -1);
    }
    return position;
}

} // anonymous


// TO DO: Send change to frontend when activeInput changes

KeyboardDevice::KeyboardDevice()
    : QAbstractPhysicalDeviceBackendNode(QBackendNode::ReadOnly)
    , m_inputHandler(nullptr)
{
    m_keyStates.keys[0] = 0;
    m_keyStates.keys[1] = 0;
    m_keyStates.keys[2] = 0;
    m_keyStates.keys[3] = 0;
    m_keyStates.keys[4] = 0;
}

void KeyboardDevice::cleanup()
{
    QAbstractPhysicalDeviceBackendNode::cleanup();
    m_keyStates.keys[0] = 0;
    m_keyStates.keys[1] = 0;
    m_keyStates.keys[2] = 0;
    m_keyStates.keys[3] = 0;
    m_keyStates.keys[4] = 0;
}

void KeyboardDevice::requestFocusForInput(Qt3DCore::QNodeId inputId)
{
    // Saves the last inputId, this will then be used in an Aspect Job to determine which
    // input will have the focus. This in turn will call KeyboardInput::setFocus which will
    // decide if sending a notification to the frontend is necessary
    m_lastRequester = inputId;
}

void KeyboardDevice::setInputHandler(InputHandler *handler)
{
    m_inputHandler = handler;
}

void KeyboardDevice::setCurrentFocusItem(Qt3DCore::QNodeId input)
{
    m_currentFocusItem = input;
}

float KeyboardDevice::axisValue(int axisIdentifier) const
{
    Q_UNUSED(axisIdentifier);
    return 0.0f;
}

bool KeyboardDevice::isButtonPressed(int buttonIdentifier) const
{
    QPair<int, int> position = getKeyPosition(buttonIdentifier);
    if (position.first != -1 && position.second != -1)
        return m_keyStates.keys[position.first] & (1 << position.second);
    return false;
}

void KeyboardDevice::setButtonValue(int key, bool value)
{
    QPair<int, int> position = getKeyPosition(key);
    if (position.first != -1 && position.second != -1) {
        if (value)
            m_keyStates.keys[position.first] |= (1 << position.second);
        else
            m_keyStates.keys[position.first] &= ~(1 << position.second);
    }
}

void KeyboardDevice::updateKeyEvents(const QList<QT_PREPEND_NAMESPACE(QKeyEvent)> &events)
{
    for (const QT_PREPEND_NAMESPACE(QKeyEvent) &e : events)
        setButtonValue(e.key(), e.type() == QT_PREPEND_NAMESPACE(QKeyEvent)::KeyPress ? true : false);
}


void KeyboardDevice::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &)
{
}

KeyboardDeviceFunctor::KeyboardDeviceFunctor(QInputAspect *inputaspect, InputHandler *handler)
    : m_inputAspect(inputaspect)
    , m_handler(handler)
{
}

Qt3DCore::QBackendNode *KeyboardDeviceFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    KeyboardDevice *keyboardDevice = m_handler->keyboardDeviceManager()->getOrCreateResource(change->subjectId());
    keyboardDevice->setInputAspect(m_inputAspect);
    keyboardDevice->setInputHandler(m_handler);
    m_handler->appendKeyboardDevice(m_handler->keyboardDeviceManager()->lookupHandle(change->subjectId()));
    return keyboardDevice;
}

Qt3DCore::QBackendNode *KeyboardDeviceFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_handler->keyboardDeviceManager()->lookupResource(id);
}

void KeyboardDeviceFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_handler->removeKeyboardDevice(m_handler->keyboardDeviceManager()->lookupHandle(id));
    m_handler->keyboardDeviceManager()->releaseResource(id);
}

} // namespace Inputs
} // namespace Qt3DInput

QT_END_NAMESPACE
