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

#include "qinputdeviceintegration_p.h"
#include "qinputdeviceintegration_p_p.h"

#include <Qt3DInput/QInputAspect>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

QInputDeviceIntegrationPrivate::QInputDeviceIntegrationPrivate()
    : QObjectPrivate()
    , m_aspect(nullptr)
{
}

/*!
    \class Qt3DInput::QInputDeviceIntegration
    \inmodule Qt3DInput
    \since 5.5
    \brief Abstract base class used to define new input methods such as game controllers

*/

/*!
   Create a new QInputDeviceIntegration with parent /a parent
 */
QInputDeviceIntegration::QInputDeviceIntegration(QObject *parent)
    : QObject(*new QInputDeviceIntegrationPrivate, parent)
{
}

/*! \internal */
QInputDeviceIntegration::QInputDeviceIntegration(QInputDeviceIntegrationPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

/*!
   Register a corresponding backend class for this front end implementation
 */
void QInputDeviceIntegration::registerBackendType(const QMetaObject &metaObject, const Qt3DCore::QBackendNodeMapperPtr &functor)
{
    Q_D(QInputDeviceIntegration);
    d->m_aspect->registerBackendType(metaObject, functor);
}

/*!
   Called by the InputAspect object after the Integration has been created
 */
void QInputDeviceIntegration::initialize(QInputAspect *aspect)
{
    Q_D(QInputDeviceIntegration);
    d->m_aspect = aspect;
    onInitialize();
}

/*!
 * \brief QInputDeviceIntegration::inputAspect
 * \return the Input Aspect associated with the InputDeviceIntegration
 */
QInputAspect *QInputDeviceIntegration::inputAspect() const
{
    Q_D(const QInputDeviceIntegration);
    return d->m_aspect;
}

} // namespace Qt3DInput

/*!
  \fn QInputDeviceIntegration::createPhysicalDevice(const QString &name)

  Create the Physical device identified by \a name.

  If not recognized return Q_NULLPTR
*/

/*!
  \fn QInputDeviceIntegration::physicalDevices() const

 \return the list of node ids for physical devices associated with this QInputDeviceIntegration.
*/

/*!
  \fn QInputDeviceIntegration::physicalDevice(Qt3DCore::QNodeId id) const

   \return the QAbstractPhysicalDevice identified by the given id if it is related to this QInputDeviceIntegration.
*/

QT_END_NAMESPACE
