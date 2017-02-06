/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "mousehandler_p.h"
#include "inputmanagers_p.h"
#include "inputhandler_p.h"
#include "mousedevice_p.h"

#include <Qt3DInput/qmousehandler.h>
#include <Qt3DInput/private/qmousehandler_p.h>
#include <Qt3DInput/qmousedevice.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DInput {
namespace Input {

MouseHandler::MouseHandler()
    : QBackendNode(ReadWrite)
    , m_inputHandler(nullptr)
{
}

MouseHandler::~MouseHandler()
{
}

void MouseHandler::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QMouseHandlerData>>(change);
    const auto &data = typedChange->data;
    setDevice(data.mouseDeviceId);
}

Qt3DCore::QNodeId MouseHandler::mouseDevice() const
{
    return m_mouseDevice;
}

void MouseHandler::setInputHandler(InputHandler *handler)
{
    m_inputHandler = handler;
}

void MouseHandler::mouseEvent(const QMouseEventPtr &event)
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("mouse");
    e->setValue(QVariant::fromValue(event));
    notifyObservers(e);
}

void MouseHandler::wheelEvent(const QWheelEventPtr &event)
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("wheel");
    e->setValue(QVariant::fromValue(event));
    notifyObservers(e);
}

void MouseHandler::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == PropertyUpdated) {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("device")) {
            const QNodeId newId = propertyChange->value().value<QNodeId>();
            if (m_mouseDevice != newId) {
                setDevice(newId);
            }
        }
    }
    QBackendNode::sceneChangeEvent(e);
}

void MouseHandler::setDevice(Qt3DCore::QNodeId device)
{
    m_mouseDevice = device;
}

MouseHandlerFunctor::MouseHandlerFunctor(InputHandler *handler)
    : m_handler(handler)
{
}

Qt3DCore::QBackendNode *MouseHandlerFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    MouseHandler *input = m_handler->mouseInputManager()->getOrCreateResource(change->subjectId());
    input->setInputHandler(m_handler);
    return input;
}

Qt3DCore::QBackendNode *MouseHandlerFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_handler->mouseInputManager()->lookupResource(id);
}

void MouseHandlerFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_handler->mouseInputManager()->releaseResource(id);
}

} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE
