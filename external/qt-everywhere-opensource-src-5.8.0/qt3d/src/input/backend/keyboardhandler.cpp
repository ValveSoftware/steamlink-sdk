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

#include "keyboardhandler_p.h"
#include <Qt3DInput/qkeyboardhandler.h>
#include <Qt3DInput/private/qkeyboardhandler_p.h>
#include "qkeyboarddevice.h"
#include "inputhandler_p.h"
#include "inputmanagers_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <QVariant>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DInput {
namespace Input {

KeyboardHandler::KeyboardHandler()
    : QBackendNode(QBackendNode::ReadWrite)
    , m_inputHandler(nullptr)
    , m_focus(false)
{
}

void KeyboardHandler::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QKeyboardHandlerData>>(change);
    const auto &data = typedChange->data;
    setSourcerDevice(data.keyboardDeviceId);
    m_focus = false;
    if (data.focus)
        requestFocus();
}

Qt3DCore::QNodeId KeyboardHandler::keyboardDevice() const
{
    return m_keyboardDevice;
}

void KeyboardHandler::setInputHandler(InputHandler *handler)
{
    m_inputHandler = handler;
}

// Called by the KeyboadDevice when the focus for the KeyboardHandler has changed
// Sends a change notification so that the frontend can update itself
void KeyboardHandler::setFocus(bool focus)
{
    if (focus != m_focus) {
        m_focus = focus;
        auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
        e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
        e->setPropertyName("focus");
        e->setValue(m_focus);
        notifyObservers(e);
    }
}

void KeyboardHandler::keyEvent(const QKeyEventPtr &event)
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("event");
    e->setValue(QVariant::fromValue(event));
    notifyObservers(e);
}

void KeyboardHandler::sceneChangeEvent(const QSceneChangePtr &e)
{
    bool focusRequest = false;
    if (e->type() == PropertyUpdated) {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("sourceDevice")) {
            const QNodeId newId = propertyChange->value().value<QNodeId>();
            if (m_keyboardDevice != newId) {
                setSourcerDevice(newId);
                focusRequest = m_focus;
            }
        } else if (propertyChange->propertyName() == QByteArrayLiteral("focus")) {
            focusRequest = propertyChange->value().toBool();
        }
    }
    if (focusRequest)
        requestFocus();
    QBackendNode::sceneChangeEvent(e);
}

void KeyboardHandler::requestFocus()
{
    KeyboardDevice *keyboardDevice = m_inputHandler->keyboardDeviceManager()->lookupResource(m_keyboardDevice);
    if (keyboardDevice && isEnabled())
        keyboardDevice->requestFocusForInput(peerId());
}

void KeyboardHandler::setSourcerDevice(QNodeId device)
{
    m_keyboardDevice = device;
}

KeyboardHandlerFunctor::KeyboardHandlerFunctor(InputHandler *handler)
    : m_handler(handler)
{
}

Qt3DCore::QBackendNode *KeyboardHandlerFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    KeyboardHandler *input = m_handler->keyboardInputManager()->getOrCreateResource(change->subjectId());
    input->setInputHandler(m_handler);
    return input;
}

QBackendNode *KeyboardHandlerFunctor::get(QNodeId id) const
{
    return m_handler->keyboardInputManager()->lookupResource(id);
}

void KeyboardHandlerFunctor::destroy(QNodeId id) const
{
    m_handler->keyboardInputManager()->releaseResource(id);
}

} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE
