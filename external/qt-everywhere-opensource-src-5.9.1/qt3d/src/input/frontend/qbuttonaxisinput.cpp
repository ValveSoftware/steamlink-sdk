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

#include "qbuttonaxisinput.h"
#include "qbuttonaxisinput_p.h"

#include <Qt3DInput/qabstractphysicaldevice.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

/*!
 * \qmltype ButtonAxisInput
 * \instantiates Qt3DInput::QButtonAxisInput
 * \inqmlmodule Qt3D.Input
 * \brief QML frontend for QButtonAxisInput C++ class.
 * \since 5.7
 * The axis value is controlled by buttons rather than a traditional analog input such as a joystick.
 *
 */

/*!
 * \class Qt3DInput::QButtonAxisInput
 * \inheaderfile Qt3DInput/QButtonAxisInput
 * \inmodule Qt3DInput
 * \brief An axis input controlled by buttons
 * \since 5.7
 * The axis value is controlled by buttons rather than a traditional analog input such as a joystick.
 *
 */
/*!
    \qmlproperty real ButtonAxisInput::scale
    Specifies how the axis output value is scaled. No scaling is applied by default
    so it defaults to 1.
*/

/*!
    \qmlproperty list<var> ButtonAxisInput::buttons
    Specifies the buttons which control the axis
*/

/*!
    \qmlproperty real ButtonAxisInput::acceleration
    Specifies the rate at which the axis value changes when a button is held
*/

/*!
    \qmlproperty list<var> ButtonAxisInput::deceleration
    Specifies the rate at which the axis value returns to normal when a button is released
*/

/*!
    Constructs a new QButtonAxisInput instance with \a parent.
 */
QButtonAxisInput::QButtonAxisInput(Qt3DCore::QNode *parent)
    : QAbstractAxisInput(*new QButtonAxisInputPrivate, parent)
{
}

/*! \internal */
QButtonAxisInput::~QButtonAxisInput()
{
}

/*!
    \property QButtonAxisInput::scale
    Specifies how the axis output value is scaled. No scaling is applied by default
    so it defaults to 1.
 */
void QButtonAxisInput::setScale(float scale)
{
    Q_D(QButtonAxisInput);
    if (d->m_scale != scale) {
        d->m_scale = scale;
        emit scaleChanged(scale);
    }
}

float QButtonAxisInput::scale() const
{
    Q_D(const QButtonAxisInput);
    return d->m_scale;
}

/*!
    \property QButtonAxisInput::buttons
    Specifies the buttons which control the axis
 */
void QButtonAxisInput::setButtons(const QVector<int> &buttons)
{
    Q_D(QButtonAxisInput);
    if (buttons != d->m_buttons) {
        d->m_buttons = buttons;
        emit buttonsChanged(buttons);
    }
}

QVector<int> QButtonAxisInput::buttons() const
{
    Q_D(const QButtonAxisInput);
    return d->m_buttons;
}

/*!
    \property QButtonAxisInput::acceleration
    Specifies the rate at which the axis value changes when a button is held
 */
float QButtonAxisInput::acceleration() const
{
    Q_D(const QButtonAxisInput);
    return d->m_acceleration;
}

void QButtonAxisInput::setAcceleration(float acceleration)
{
    Q_D(QButtonAxisInput);
    if (d->m_acceleration != acceleration) {
        d->m_acceleration = acceleration;
        emit accelerationChanged(acceleration);
    }
}

/*!
    \property QButtonAxisInput::deceleration
    Specifies the rate at which the axis value returns to normal when a button is released
 */
float QButtonAxisInput::deceleration() const
{
    Q_D(const QButtonAxisInput);
    return d->m_deceleration;
}

void QButtonAxisInput::setDeceleration(float deceleration)
{
    Q_D(QButtonAxisInput);
    if (d->m_deceleration != deceleration) {
        d->m_deceleration = deceleration;
        emit decelerationChanged(deceleration);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QButtonAxisInput::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QButtonAxisInputData>::create(this);
    auto &data = creationChange->data;

    Q_D(const QButtonAxisInput);
    data.sourceDeviceId = qIdForNode(d->m_sourceDevice);
    data.buttons = d->m_buttons;
    data.scale = d->m_scale;
    data.acceleration = d->m_acceleration;
    data.deceleration = d->m_deceleration;

    return creationChange;
}

} // Qt3DInput

QT_END_NAMESPACE
