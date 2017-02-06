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

#include "qabstractaxisinput.h"
#include "qabstractaxisinput_p.h"
#include <Qt3DCore/qnodecreatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

/*!
 * \qmltype AbstractAxisInput
 * \inqmlmodule Qt3D.Input
 * \brief QML frontend for abstract QAbstractAxisInput C++ class.
 * \since 5.5
 *
 */

/*!
 * \class Qt3DInput::QAbstractAxisInput
 * \inmodule Qt3DInput
 * \brief QAbstractActionInput is the base class for all Axis Input.
 * \since 5.5
 *
 */

/*!
    \qmlproperty AbstractPhysicalDevice Qt3D.Input::AbstractAxisInput::sourceDevice
*/

/*!
    \internal
 */
QAbstractAxisInput::QAbstractAxisInput(QAbstractAxisInputPrivate &dd, Qt3DCore::QNode *parent)
    : QNode(dd, parent)
{
}

/*!
    \property Qt3DInput::QAbstractAxisInput::sourceDevice

    The source device for the QAbstractAxisInput.
*/

void QAbstractAxisInput::setSourceDevice(QAbstractPhysicalDevice *sourceDevice)
{
    Q_D(QAbstractAxisInput);
    if (d->m_sourceDevice != sourceDevice) {

        if (d->m_sourceDevice)
            d->unregisterDestructionHelper(d->m_sourceDevice);

        // We need to add it as a child of the current node if it has been declared inline
        // Or not previously added as a child of the current node so that
        // 1) The backend gets notified about it's creation
        // 2) When the current node is destroyed, it gets destroyed as well
        if (sourceDevice && !sourceDevice->parent())
            sourceDevice->setParent(this);

        d->m_sourceDevice = sourceDevice;
        // Ensures proper bookkeeping
        if (d->m_sourceDevice)
            d->registerDestructionHelper(sourceDevice, &QAbstractAxisInput::setSourceDevice, d->m_sourceDevice);

        emit sourceDeviceChanged(sourceDevice);
    }
}

/*!
    \internal
 */
QAbstractAxisInput::~QAbstractAxisInput()
{
}

QAbstractPhysicalDevice *QAbstractAxisInput::sourceDevice() const
{
    Q_D(const QAbstractAxisInput);
    return d->m_sourceDevice;
}

} // Qt3DInput

QT_END_NAMESPACE
