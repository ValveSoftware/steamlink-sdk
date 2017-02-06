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

#include "actioninput_p.h"
#include <Qt3DInput/qactioninput.h>
#include <Qt3DInput/private/qactioninput_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DInput/private/utils_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

ActionInput::ActionInput()
    : AbstractActionInput()
    , m_buttons(0)
{
}

void ActionInput::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QActionInputData>>(change);
    const auto &data = typedChange->data;
    m_buttons = data.buttons;
    m_sourceDevice = data.sourceDeviceId;
}

void ActionInput::cleanup()
{
    setEnabled(false);
    m_sourceDevice = Qt3DCore::QNodeId();
    m_buttons.clear();
}

void ActionInput::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == Qt3DCore::PropertyUpdated) {
        Qt3DCore::QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("sourceDevice"))
            m_sourceDevice = propertyChange->value().value<Qt3DCore::QNodeId>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("buttons"))
            m_buttons = propertyChange->value().value<QVector<int>>();
    }
    AbstractActionInput::sceneChangeEvent(e);
}

bool ActionInput::process(InputHandler *inputHandler, qint64 currentTime)
{
    Q_UNUSED(currentTime);

    if (!isEnabled())
        return false;

    QAbstractPhysicalDeviceBackendNode *physicalDeviceBackend = Utils::physicalDeviceForInput(this, inputHandler);
    if (!physicalDeviceBackend)
        return false;

    for (int button : qAsConst(m_buttons)) {
        if (physicalDeviceBackend->isButtonPressed(button))
            return true;
    }

    return false;
}

} // namespace Input

} // namespace Qt3DInput

QT_END_NAMESPACE

