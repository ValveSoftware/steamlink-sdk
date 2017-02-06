/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qabstractphysicaldeviceproxy_p.h"
#include "qabstractphysicaldeviceproxy_p_p.h"
#include <Qt3DInput/qphysicaldevicecreatedchange.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

/*!
    \internal
 */
QAbstractPhysicalDeviceProxyPrivate::QAbstractPhysicalDeviceProxyPrivate(const QString &deviceName)
    : QAbstractPhysicalDevicePrivate()
    , m_deviceName(deviceName)
    , m_status(QAbstractPhysicalDeviceProxy::NotFound)
    , m_device(nullptr)
{
}

/*!
    \internal
 */
QAbstractPhysicalDeviceProxyPrivate::~QAbstractPhysicalDeviceProxyPrivate()
{
}

/*!
    \internal
 */
void QAbstractPhysicalDeviceProxyPrivate::setStatus(QAbstractPhysicalDeviceProxy::DeviceStatus status)
{
    if (status != m_status) {
        m_status = status;
        emit q_func()->statusChanged(status);
    }
}

/*!
    \class Qt3DInput::QAbstractPhysicalDeviceProxy
    \inmodule Qt3DInput

    \brief Qt3DInput::QAbstractPhysicalDeviceProxy acts as a proxy
    for an actual Qt3DInput::QQAbstractPhysicalDevice device.

    Qt3DInput::QAbstractPhysicalDeviceProxy can be used to facilitate
    exposing a physical device to users. It alleviates the need to introspect
    the axis and buttons based on their names.

    It is typcally used through subclassing allowing to set the device name and
    defining enums for the various axis and buttons of your targeted device.

    At runtime, the status property will be updated to reflect whether an
    actual device matching the device name could be created.

    \since 5.8
 */

QString QAbstractPhysicalDeviceProxy::deviceName() const
{
    Q_D(const QAbstractPhysicalDeviceProxy);
    return d->m_deviceName;
}

QAbstractPhysicalDeviceProxy::DeviceStatus QAbstractPhysicalDeviceProxy::status() const
{
    Q_D(const QAbstractPhysicalDeviceProxy);
    return d->m_status;
}

int QAbstractPhysicalDeviceProxy::axisCount() const
{
    Q_D(const QAbstractPhysicalDeviceProxy);
    if (d->m_device != nullptr)
        return d->m_device->axisCount();
    return 0;
}

int QAbstractPhysicalDeviceProxy::buttonCount() const
{
    Q_D(const QAbstractPhysicalDeviceProxy);
    if (d->m_device != nullptr)
        return d->m_device->buttonCount();
    return 0;
}

QStringList QAbstractPhysicalDeviceProxy::axisNames() const
{
    Q_D(const QAbstractPhysicalDeviceProxy);
    if (d->m_device != nullptr)
        return d->m_device->axisNames();
    return QStringList();
}

QStringList QAbstractPhysicalDeviceProxy::buttonNames() const
{
    Q_D(const QAbstractPhysicalDeviceProxy);
    if (d->m_device != nullptr)
        return d->m_device->buttonNames();
    return QStringList();
}

int QAbstractPhysicalDeviceProxy::axisIdentifier(const QString &name) const
{
    Q_D(const QAbstractPhysicalDeviceProxy);
    if (d->m_device != nullptr)
        return d->m_device->axisIdentifier(name);
    return -1;
}

int QAbstractPhysicalDeviceProxy::buttonIdentifier(const QString &name) const
{
    Q_D(const QAbstractPhysicalDeviceProxy);
    if (d->m_device != nullptr)
        return d->m_device->buttonIdentifier(name);
    return -1;
}

/*!
    \internal
 */
void QAbstractPhysicalDeviceProxy::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_D(QAbstractPhysicalDeviceProxy);
    Qt3DCore::QPropertyUpdatedChangePtr e = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(change);
    if (e->type() == Qt3DCore::PropertyUpdated) {
        if (e->propertyName() == QByteArrayLiteral("device")) {
            QAbstractPhysicalDevice *device = e->value().value<Qt3DInput::QAbstractPhysicalDevice *>();
            QAbstractPhysicalDevice *oldDevice = d->m_device;
            setDevice(device);
            // Delete the old device if it existed
            if (oldDevice != nullptr)
                delete oldDevice;
        }
    }
    QAbstractPhysicalDevice::sceneChangeEvent(change);
}

/*!
    \internal
 */
QAbstractPhysicalDeviceProxy::QAbstractPhysicalDeviceProxy(QAbstractPhysicalDeviceProxyPrivate &dd, Qt3DCore::QNode *parent)
    : QAbstractPhysicalDevice(dd, parent)
{
}

/*!
    \internal
 */
Qt3DCore::QNodeCreatedChangeBasePtr QAbstractPhysicalDeviceProxy::createNodeCreationChange() const
{
    auto creationChange = QPhysicalDeviceCreatedChangePtr<QAbstractPhysicalDeviceProxyData>::create(this);
    QAbstractPhysicalDeviceProxyData &data = creationChange->data;

    Q_D(const QAbstractPhysicalDeviceProxy);
    data.deviceName = d->m_deviceName;

    return creationChange;
}

/*!
    \internal
 */
void QAbstractPhysicalDeviceProxy::setDevice(QAbstractPhysicalDevice *device)
{
    Q_D(QAbstractPhysicalDeviceProxy);

    // Note: technically book keeping could be optional since we are the parent
    // of the device. But who knows if someone plays with the object tree...

    // Unset bookkeeper
    if (d->m_device != nullptr) {
        // Note: we cannot delete the device here as we don't how if we are
        // called by the bookkeeper (in which case we would do a double free)
        // or by the sceneChangeEvent
        d->unregisterDestructionHelper(d->m_device);
        d->setStatus(QAbstractPhysicalDeviceProxy::NotFound);
    }

    // Set parent so that node is created in the backend
    if (device != nullptr && device->parent() == nullptr)
        device->setParent(this);

    d->m_device = device;

    // Set bookkeeper
    if (device != nullptr) {
       d->setStatus(QAbstractPhysicalDeviceProxy::Ready);
       d->registerDestructionHelper(d->m_device, &QAbstractPhysicalDeviceProxy::setDevice, d->m_device);
    }
}

} // Qt3DInput

QT_END_NAMESPACE
