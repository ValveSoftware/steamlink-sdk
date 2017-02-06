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

#include "qanalogaxisinput.h"
#include "qanalogaxisinput_p.h"
#include <Qt3DInput/qabstractphysicaldevice.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

/*!
 * \qmltype AnalogAxisInput
 * \instantiates Qt3DInput::QAnalogAxisInput
 * \inqmlmodule Qt3D.Input
 * \brief QML frontend for QAnalogAxisInput C++ class.
 * \since 5.7
 * \TODO
 *
 */

/*!
 * \class Qt3DInput::QAnalogAxisInput
 * \inmodule Qt3DInput
 * \brief An axis input controlled by an analog input
 * \since 5.7
 * The axis value is controlled like a traditional analog input such as a joystick.
 *
 *
 */

/*!
    \qmlproperty int AnalogAxisInput::axis

    Holds the axis for the AnalogAxisInput.
*/


/*!
    Constructs a new QAnalogAxisInput instance with \a parent.
 */
QAnalogAxisInput::QAnalogAxisInput(Qt3DCore::QNode *parent)
    : QAbstractAxisInput(*new QAnalogAxisInputPrivate, parent)
{
}

/*! \internal */
QAnalogAxisInput::~QAnalogAxisInput()
{
}

/*!
  \property Qt3DInput::QAnalogAxisInput::axis

  Axis for the analog input.

  \sa Qt3DInput::QMouseDevice::Axis
*/
void QAnalogAxisInput::setAxis(int axis)
{
    Q_D(QAnalogAxisInput);
    if (d->m_axis != axis) {
        d->m_axis = axis;
        emit axisChanged(axis);
    }
}

int QAnalogAxisInput::axis() const
{
    Q_D(const QAnalogAxisInput);
    return d->m_axis;
}

Qt3DCore::QNodeCreatedChangeBasePtr QAnalogAxisInput::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QAnalogAxisInputData>::create(this);
    auto &data = creationChange->data;

    Q_D(const QAnalogAxisInput);
    data.sourceDeviceId = qIdForNode(d->m_sourceDevice);
    data.axis = d->m_axis;

    return creationChange;
}

} // Qt3DInput

QT_END_NAMESPACE
