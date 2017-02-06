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

#include "qabstractphysicaldevice.h"
#include "qabstractphysicaldevice_p.h"
#include <Qt3DInput/qphysicaldevicecreatedchange.h>
#include <Qt3DInput/qaxissetting.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

/*! \internal */
QAbstractPhysicalDevicePrivate::QAbstractPhysicalDevicePrivate()
    : m_axisSettings()
{
}

/*! \internal */
QAbstractPhysicalDevicePrivate::~QAbstractPhysicalDevicePrivate()
{
}

/*!
    \class Qt3DInput::QAbstractPhysicalDevice
    \inmodule Qt3DInput
    \inherits Qt3DCore::QNode
    \brief QAbstractPhysicalDevice is the base class used by Qt3d to interact with arbitrary input devices.

    \since 5.6
*/

/*!
    \qmltype AbstractPhysicalDevice
    \inqmlmodule Qt3D.Input
    \instantiates Qt3DInput::QAbstractPhysicalDevice
    \brief QML frontend for the abstract Qt3DInput::QAbstractPhysicalDevice C++ class.

    The base class used by Qt3d to interact with arbitrary input devices.

    \since 5.6
*/

/*!
    Constructs a new QAbstractPhysicalDevice instance with \a parent.
 */
QAbstractPhysicalDevice::QAbstractPhysicalDevice(Qt3DCore::QNode *parent)
    : Qt3DCore::QNode(*new QAbstractPhysicalDevicePrivate, parent)
{
}

/*! \internal */
QAbstractPhysicalDevice::~QAbstractPhysicalDevice()
{
}

QAbstractPhysicalDevice::QAbstractPhysicalDevice(QAbstractPhysicalDevicePrivate &dd, Qt3DCore::QNode *parent)
    : Qt3DCore::QNode(dd, parent)
{
}

/*!
    \return the number of axis this device has.
 */
int QAbstractPhysicalDevice::axisCount() const
{
    Q_D(const QAbstractPhysicalDevice);
    return d->m_axesHash.size();
}

/*!
    \return the number of buttons this device has.
 */
int QAbstractPhysicalDevice::buttonCount() const
{
    Q_D(const QAbstractPhysicalDevice);
    return d->m_buttonsHash.size();
}

/*!
    \return a list of the names of device's axis.
 */
QStringList QAbstractPhysicalDevice::axisNames() const
{
    Q_D(const QAbstractPhysicalDevice);
    return d->m_axesHash.keys();
}

/*!
    \return a list of the names of device's buttons.
 */
QStringList QAbstractPhysicalDevice::buttonNames() const
{
    Q_D(const QAbstractPhysicalDevice);
    return d->m_buttonsHash.keys();
}

/*!
    \return the integer identifer of the axis \a name or -1 if it does not exist on this device.
 */
int QAbstractPhysicalDevice::axisIdentifier(const QString &name) const
{
    Q_D(const QAbstractPhysicalDevice);
    auto it = d->m_axesHash.find(name);
    if (it != d->m_axesHash.end())
        return *it;
    return -1;
}

/*!
    \return the integer identifer of the button \a name or -1 if it does not exist on this device.
 */
int QAbstractPhysicalDevice::buttonIdentifier(const QString &name) const
{
    Q_D(const QAbstractPhysicalDevice);
    auto it = d->m_buttonsHash.find(name);
    if (it != d->m_buttonsHash.end())
        return *it;
    return -1;
}

/*!
    Add the axisSetting \a axisSetting to this device.
 */
void QAbstractPhysicalDevice::addAxisSetting(QAxisSetting *axisSetting)
{
    Q_D(QAbstractPhysicalDevice);
    if (axisSetting && !d->m_axisSettings.contains(axisSetting)) {
        if (d->m_changeArbiter) {
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(id(), axisSetting);
            change->setPropertyName("axisSettings");
            d->notifyObservers(change);
        }

        d->m_axisSettings.push_back(axisSetting);
    }
}

/*!
    Remove the axisSetting \a axisSetting to this device.
 */
void QAbstractPhysicalDevice::removeAxisSetting(QAxisSetting *axisSetting)
{
    Q_D(QAbstractPhysicalDevice);
    if (axisSetting && d->m_axisSettings.contains(axisSetting)) {
        if (d->m_changeArbiter) {
            const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(id(), axisSetting);
            change->setPropertyName("axisSettings");
            d->notifyObservers(change);
        }

        d->m_axisSettings.removeOne(axisSetting);
    }
}

/*!
    \return the axisSettings associated with this device.
 */
QVector<QAxisSetting *> QAbstractPhysicalDevice::axisSettings() const
{
    Q_D(const QAbstractPhysicalDevice);
    return d->m_axisSettings;
}

/*!
    Used to notify observers that an axis value has been changed.
 */
void QAbstractPhysicalDevicePrivate::postAxisEvent(int axis, qreal value)
{
    Q_Q(QAbstractPhysicalDevice);
    Qt3DCore::QPropertyUpdatedChangePtr change(new Qt3DCore::QPropertyUpdatedChange(q->id()));
    change->setPropertyName("axisEvent");
    change->setValue(QVariant::fromValue(QPair<int, qreal>(axis, value)));
    notifyObservers(change);
}

/*!
    Used to notify observers that an button value has been changed.
 */
void QAbstractPhysicalDevicePrivate::postButtonEvent(int button, qreal value)
{
    Q_Q(QAbstractPhysicalDevice);
    Qt3DCore::QPropertyUpdatedChangePtr change(new Qt3DCore::QPropertyUpdatedChange(q->id()));
    change->setPropertyName("buttonEvent");
    change->setValue(QVariant::fromValue(QPair<int, qreal>(button, value)));
    notifyObservers(change);
}

Qt3DCore::QNodeCreatedChangeBasePtr QAbstractPhysicalDevice::createNodeCreationChange() const
{
    auto creationChange = QPhysicalDeviceCreatedChangeBasePtr::create(this);
    return creationChange;
}

}

QT_END_NAMESPACE
