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

#include "qdynamicpropertyupdatedchange.h"
#include "qdynamicpropertyupdatedchange_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QDynamicPropertyUpdatedChangePrivate::QDynamicPropertyUpdatedChangePrivate()
    : QPropertyUpdatedChangeBasePrivate()
{
}

QDynamicPropertyUpdatedChangePrivate::~QDynamicPropertyUpdatedChangePrivate()
{
}

/*!
 * \class Qt3DCore::QDynamicPropertyUpdatedChange
 * \inherits Qt3DCore::QPropertyUpdatedChangeBase
 * \inmodule Qt3DCore
 * \brief The QDynamicPropertyUpdatedChange class is used to notify when a dynamic property value is updated
 *
 */

/*!
 * \typedef Qt3DCore::QDynamicPropertyUpdatedChangePtr
 * \relates Qt3DCore::QDynamicPropertyUpdatedChange
 *
 * A shared pointer for QDynamicPropertyUpdatedChange.
 */

/*!
 * Constructs a new QDynamicPropertyUpdatedChange with \a subjectId.
 */
QDynamicPropertyUpdatedChange::QDynamicPropertyUpdatedChange(QNodeId subjectId)
    : QPropertyUpdatedChangeBase(*new QDynamicPropertyUpdatedChangePrivate, subjectId)
{
}

QDynamicPropertyUpdatedChange::QDynamicPropertyUpdatedChange(QDynamicPropertyUpdatedChangePrivate &dd,
                                                       QNodeId subjectId)
    : QPropertyUpdatedChangeBase(dd, subjectId)
{
}

QDynamicPropertyUpdatedChange::~QDynamicPropertyUpdatedChange()
{
}

/*!
 * \return name of the property.
 */
QByteArray QDynamicPropertyUpdatedChange::propertyName() const
{
    Q_D(const QDynamicPropertyUpdatedChange);
    return d->m_propertyName;
}

/*!
 * Sets the property change \a name.
 */
void QDynamicPropertyUpdatedChange::setPropertyName(const QByteArray &name)
{
    Q_D(QDynamicPropertyUpdatedChange);
    d->m_propertyName = name;
}

/*!
 * \return property value.
 */
QVariant QDynamicPropertyUpdatedChange::value() const
{
    Q_D(const QDynamicPropertyUpdatedChange);
    return d->m_value;
}

/*!
 * Set the property change \a value.
 */
void QDynamicPropertyUpdatedChange::setValue(const QVariant &value)
{
    Q_D(QDynamicPropertyUpdatedChange);
    d->m_value = value;
}

} // namespace Qt3DCore

QT_END_NAMESPACE
