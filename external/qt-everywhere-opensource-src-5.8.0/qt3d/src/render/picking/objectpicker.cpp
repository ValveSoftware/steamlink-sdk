/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire paul.lemire350@gmail.com
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

#include "objectpicker_p.h"
#include "qpickevent.h"
#include <Qt3DRender/qobjectpicker.h>
#include <Qt3DRender/private/qobjectpicker_p.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

ObjectPicker::ObjectPicker()
    : BackendNode(QBackendNode::ReadWrite)
    , m_isDirty(false)
    , m_isPressed(false)
    , m_hoverEnabled(false)
    , m_dragEnabled(false)
{
}

ObjectPicker::~ObjectPicker()
{
}

void ObjectPicker::cleanup()
{
    BackendNode::setEnabled(false);
    m_isDirty = false;
    m_isPressed = false;
    m_hoverEnabled = false;
    m_dragEnabled = false;
}

void ObjectPicker::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QObjectPickerData>>(change);
    const auto &data = typedChange->data;
    m_hoverEnabled = data.hoverEnabled;
    m_dragEnabled = data.dragEnabled;
    m_isDirty = true;
}

void ObjectPicker::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == Qt3DCore::PropertyUpdated) {
        const Qt3DCore::QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);

        if (propertyChange->propertyName() == QByteArrayLiteral("hoverEnabled")) {
            m_hoverEnabled = propertyChange->value().toBool();
            m_isDirty = true;
        } else if (propertyChange->propertyName() == QByteArrayLiteral("dragEnabled")) {
            m_dragEnabled = propertyChange->value().toBool();
            m_isDirty = true;
        }
        markDirty(AbstractRenderer::AllDirty);
    }

    BackendNode::sceneChangeEvent(e);
}

bool ObjectPicker::isDirty() const
{
    return m_isDirty;
}

bool ObjectPicker::isPressed() const
{
    return m_isPressed;
}

void ObjectPicker::unsetDirty()
{
    m_isDirty = false;
}

void ObjectPicker::makeDirty()
{
    m_isDirty = true;
}

bool ObjectPicker::isHoverEnabled() const
{
    return m_hoverEnabled;
}

bool ObjectPicker::isDragEnabled() const
{
    return m_dragEnabled;
}

void ObjectPicker::onClicked(QPickEventPtr event)
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("clicked");
    e->setValue(QVariant::fromValue(event));
    notifyObservers(e);
}

void ObjectPicker::onMoved(QPickEventPtr event)
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("moved");
    e->setValue(QVariant::fromValue(event));
    notifyObservers(e);
}

void ObjectPicker::onPressed(QPickEventPtr event)
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("pressed");
    e->setValue(QVariant::fromValue(event));
    m_isPressed = true;
    notifyObservers(e);
}

void ObjectPicker::onReleased(QPickEventPtr event)
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("released");
    e->setValue(QVariant::fromValue(event));
    m_isPressed = false;
    notifyObservers(e);
}

void ObjectPicker::onEntered()
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("entered");
    notifyObservers(e);
}

void ObjectPicker::onExited()
{
    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("exited");
    notifyObservers(e);
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
