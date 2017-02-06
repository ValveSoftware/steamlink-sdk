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

#include "keyboardmousegenericdeviceintegration_p.h"
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

KeyboardMouseGenericDeviceIntegration::KeyboardMouseGenericDeviceIntegration(InputHandler *handler)
    : Qt3DInput::QInputDeviceIntegration()
    , m_handler(handler)
{
}

KeyboardMouseGenericDeviceIntegration::~KeyboardMouseGenericDeviceIntegration()
{
}

void KeyboardMouseGenericDeviceIntegration::onInitialize()
{
}

QVector<Qt3DCore::QAspectJobPtr> KeyboardMouseGenericDeviceIntegration::jobsToExecute(qint64 time)
{
    Q_UNUSED(time)
    return QVector<Qt3DCore::QAspectJobPtr>();
}

QAbstractPhysicalDevice *KeyboardMouseGenericDeviceIntegration::createPhysicalDevice(const QString &name)
{
    Q_UNUSED(name)
    return nullptr;
}

QVector<Qt3DCore::QNodeId> KeyboardMouseGenericDeviceIntegration::physicalDevices() const
{
    // TO DO: could return the ids of active KeyboardDevice/MouseDevice
    return QVector<Qt3DCore::QNodeId>();
}

QAbstractPhysicalDeviceBackendNode *KeyboardMouseGenericDeviceIntegration::physicalDevice(Qt3DCore::QNodeId id) const
{
    QAbstractPhysicalDeviceBackendNode *device = m_handler->keyboardDeviceManager()->lookupResource(id);
    if (!device)
        device = m_handler->mouseDeviceManager()->lookupResource(id);
    if (!device)
        device = m_handler->genericDeviceBackendNodeManager()->lookupResource(id);
    return device;
}

QStringList KeyboardMouseGenericDeviceIntegration::deviceNames() const
{
    return QStringList() << tr("Keyboard") << tr("Mouse");
}

} // Input

} // Qt3DInput

QT_END_NAMESPACE
