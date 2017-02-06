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

#include "quick3dlogicaldevice_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DInput {
namespace Input {
namespace Quick {

Quick3DLogicalDevice::Quick3DLogicalDevice(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QAxis> Quick3DLogicalDevice::qmlAxes()
{
    return QQmlListProperty<QAxis>(this, 0,
                                   &Quick3DLogicalDevice::appendAxis,
                                   &Quick3DLogicalDevice::axesCount,
                                   &Quick3DLogicalDevice::axisAt,
                                   &Quick3DLogicalDevice::clearAxes);
}

QQmlListProperty<QAction> Quick3DLogicalDevice::qmlActions()
{
    return QQmlListProperty<QAction>(this, 0,
                                     &Quick3DLogicalDevice::appendAction,
                                     &Quick3DLogicalDevice::actionCount,
                                     &Quick3DLogicalDevice::actionAt,
                                     &Quick3DLogicalDevice::clearActions);
}

void Quick3DLogicalDevice::appendAxis(QQmlListProperty<QAxis> *list, QAxis *axes)
{
    Quick3DLogicalDevice *device = qobject_cast<Quick3DLogicalDevice *>(list->object);
    device->parentLogicalDevice()->addAxis(axes);
}

QAxis *Quick3DLogicalDevice::axisAt(QQmlListProperty<QAxis> *list, int index)
{
    Quick3DLogicalDevice *device = qobject_cast<Quick3DLogicalDevice *>(list->object);
    return device->parentLogicalDevice()->axes().at(index);
}

int Quick3DLogicalDevice::axesCount(QQmlListProperty<QAxis> *list)
{
    Quick3DLogicalDevice *device = qobject_cast<Quick3DLogicalDevice *>(list->object);
    return device->parentLogicalDevice()->axes().count();
}

void Quick3DLogicalDevice::clearAxes(QQmlListProperty<QAxis> *list)
{
    Quick3DLogicalDevice *device = qobject_cast<Quick3DLogicalDevice *>(list->object);
    const auto axes = device->parentLogicalDevice()->axes();
    for (QAxis *axis : axes)
        device->parentLogicalDevice()->removeAxis(axis);
}

void Quick3DLogicalDevice::appendAction(QQmlListProperty<QAction> *list, QAction *action)
{
    Quick3DLogicalDevice *device = qobject_cast<Quick3DLogicalDevice *>(list->object);
    device->parentLogicalDevice()->addAction(action);
}

QAction *Quick3DLogicalDevice::actionAt(QQmlListProperty<QAction> *list, int index)
{
    Quick3DLogicalDevice *device = qobject_cast<Quick3DLogicalDevice *>(list->object);
    return device->parentLogicalDevice()->actions().at(index);
}

int Quick3DLogicalDevice::actionCount(QQmlListProperty<QAction> *list)
{
    Quick3DLogicalDevice *device = qobject_cast<Quick3DLogicalDevice *>(list->object);
    return device->parentLogicalDevice()->actions().count();
}

void Quick3DLogicalDevice::clearActions(QQmlListProperty<QAction> *list)
{
    Quick3DLogicalDevice *device = qobject_cast<Quick3DLogicalDevice *>(list->object);
    const auto actions = device->parentLogicalDevice()->actions();
    for (QAction *action : actions)
        device->parentLogicalDevice()->removeAction(action);
}


} // namespace Quick
} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE
