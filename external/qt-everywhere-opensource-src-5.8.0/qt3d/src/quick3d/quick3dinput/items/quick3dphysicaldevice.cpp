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

#include "quick3dphysicaldevice_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DInput {
namespace Input {
namespace Quick {

Quick3DPhysicalDevice::Quick3DPhysicalDevice(QObject *parent) : QObject(parent)
{

}

QQmlListProperty<QAxisSetting> Quick3DPhysicalDevice::axisSettings()
{
    return QQmlListProperty<QAxisSetting>(this, 0,
                                          &Quick3DPhysicalDevice::appendAxisSetting,
                                          &Quick3DPhysicalDevice::axisSettingsCount,
                                          &Quick3DPhysicalDevice::axisSettingAt,
                                          &Quick3DPhysicalDevice::clearAxisSettings);
}

void Quick3DPhysicalDevice::appendAxisSetting(QQmlListProperty<QAxisSetting> *list, QAxisSetting *axisSetting)
{
    Quick3DPhysicalDevice *device = qobject_cast<Quick3DPhysicalDevice *>(list->object);
    device->parentPhysicalDevice()->addAxisSetting(axisSetting);
}

QAxisSetting *Quick3DPhysicalDevice::axisSettingAt(QQmlListProperty<QAxisSetting> *list, int index)
{
    Quick3DPhysicalDevice *device = qobject_cast<Quick3DPhysicalDevice *>(list->object);
    return device->parentPhysicalDevice()->axisSettings().at(index);
}

int Quick3DPhysicalDevice::axisSettingsCount(QQmlListProperty<QAxisSetting> *list)
{
    Quick3DPhysicalDevice *device = qobject_cast<Quick3DPhysicalDevice *>(list->object);
    return device->parentPhysicalDevice()->axisSettings().count();
}

void Quick3DPhysicalDevice::clearAxisSettings(QQmlListProperty<QAxisSetting> *list)
{
    Quick3DPhysicalDevice *device = qobject_cast<Quick3DPhysicalDevice *>(list->object);
    const auto axisSettings = device->parentPhysicalDevice()->axisSettings();
    for (QAxisSetting *axisSetting : axisSettings)
        device->parentPhysicalDevice()->removeAxisSetting(axisSetting);
}

} // namespace Quick
} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE
