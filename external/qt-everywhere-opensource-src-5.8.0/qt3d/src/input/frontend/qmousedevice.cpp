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

#include "qmousedevice.h"
#include "qmousedevice_p.h"
#include <Qt3DInput/qphysicaldevicecreatedchange.h>
#include <Qt3DInput/qmouseevent.h>
#include <Qt3DCore/qentity.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

/*! \internal */
QMouseDevicePrivate::QMouseDevicePrivate()
    : QAbstractPhysicalDevicePrivate()
    , m_sensitivity(0.1f)
{
}

/*!
    \qmltype MouseDevice
    \instantiates Qt3DInput::QMouseDevice
    \inqmlmodule Qt3D.Input
    \since 5.5
    \brief Delegates mouse events to the attached MouseHandler objects.

    A MouseDevice delegates mouse events from physical mouse device to
    MouseHandler objects. The sensitivity of the mouse can be controlled
    with the \l MouseDevice::sensitivity property, which specifies the rate
    in which the logical mouse coordinates change in response to physical
    movement of the mouse.

    \sa MouseHandler
 */

/*!
    \class Qt3DInput::QMouseDevice
    \inmodule Qt3DInput
    \since 5.5
    \brief Delegates mouse events to the attached MouseHandler objects.

    A QMouseDevice delegates mouse events from physical mouse device to
    QMouseHandler objects.  The sensitivity of the mouse can be controlled
    with the \l QMouseDevice::sensitivity property, which specifies the rate
    in which the logical mouse coordinates change in response to physical
    movement of the mouse.

    \sa QMouseHandler
 */

/*!
    \enum QMouseDevice::Axis

    The mouse axis.

    \value X
    \value Y
    \value WheelX
    \value WheelY

    \sa Qt3DInput::QAnalogAxisInput::setAxis
 */

/*!
    \qmlproperty real MouseDevice::sensitivity

    Holds the current sensitivity of the mouse device.
    Default is 0.1.
 */

/*!
    \property Qt3DInput::QMouseDevice::sensitivity

    Holds the sensitivity of the mouse device.
    Default is 0.1.
 */

/*!
    Constructs a new QMouseDevice instance with parent \a parent.
 */
QMouseDevice::QMouseDevice(QNode *parent)
    : QAbstractPhysicalDevice(*new QMouseDevicePrivate, parent)
{
}

/*! \internal */
QMouseDevice::~QMouseDevice()
{
}

/*!
    \return the axis count.

    \note Currently always returns 4.
 */
int QMouseDevice::axisCount() const
{
    return 4;
}

/*!
    \return the button count.

    \note Currently always returns 3.
 */
int QMouseDevice::buttonCount() const
{
    return 3;
}

/*!
    \return the names of the axis.

    \note Currently always returns StringList["X", "Y"]
 */
QStringList QMouseDevice::axisNames() const
{
    return QStringList()
            << QStringLiteral("X")
            << QStringLiteral("Y")
            << QStringLiteral("WheelX")
            << QStringLiteral("WheelY");
}

/*!
    \return the names of the buttons.

    \note Currently always returns StringList["Left", "Right", "Center"]
 */
QStringList QMouseDevice::buttonNames() const
{
    return QStringList()
            << QStringLiteral("Left")
            << QStringLiteral("Right")
            << QStringLiteral("Center");
}

/*!
    Convert axis \a name to axis identifier.
 */
int QMouseDevice::axisIdentifier(const QString &name) const
{
    if (name == QLatin1String("X"))
        return X;
    else if (name == QLatin1String("Y"))
        return Y;
    else if (name == QLatin1String("WheelX"))
        return WheelX;
    else if (name == QLatin1String("WheelY"))
        return WheelY;
    return -1;
}

int QMouseDevice::buttonIdentifier(const QString &name) const
{
    if (name == QLatin1String("Left"))
        return Qt3DInput::QMouseEvent::LeftButton;
    if (name == QLatin1String("Right"))
        return Qt3DInput::QMouseEvent::RightButton;
    if (name == QLatin1String("Center"))
        return Qt3DInput::QMouseEvent::MiddleButton;
    return -1;
}

/*!
  \property Qt3DInput::QMouseDevice::sensitivity

  The sensitivity of the device.
 */
float QMouseDevice::sensitivity() const
{
    Q_D(const QMouseDevice);
    return d->m_sensitivity;
}

void QMouseDevice::setSensitivity(float value)
{
    Q_D(QMouseDevice);
    if (qFuzzyCompare(value, d->m_sensitivity))
        return;

    d->m_sensitivity = value;
    emit sensitivityChanged(value);
}

/*! \internal */
void QMouseDevice::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_UNUSED(change);
    // TODO: To be completed as the mouse input aspect takes shape
}

Qt3DCore::QNodeCreatedChangeBasePtr QMouseDevice::createNodeCreationChange() const
{
    auto creationChange = QPhysicalDeviceCreatedChangePtr<QMouseDeviceData>::create(this);
    auto &data = creationChange->data;

    Q_D(const QMouseDevice);
    data.sensitivity = d->m_sensitivity;

    return creationChange;
}

} // namespace Qt3DInput

QT_END_NAMESPACE
