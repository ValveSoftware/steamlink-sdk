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

#include "qpropertyvalueaddedchangebase.h"
#include "qpropertyvalueaddedchangebase_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QPropertyValueAddedChangeBasePrivate::QPropertyValueAddedChangeBasePrivate()
    : QSceneChangePrivate()
{
}

QPropertyValueAddedChangeBasePrivate::~QPropertyValueAddedChangeBasePrivate()
{
}

/*!
 * \class Qt3DCore::QPropertyValueAddedChangeBase
 * \inmodule Qt3DCore
 * \brief The QPropertyValueAddedChangeBase class is the base class for all PropertyValueAdded QSceneChange events
 *
 * The QPropertyValueAddedChangeBase class is the base class for all QSceneChange events that
 * have the changeType() PropertyValueAdded. You should not need to instantiate this class.
 * Usually you should be using one of its subclasses such as QPropertyNodeAddedChange.
 *
 * You can subclass this to create your own node added types for communication between
 * your QNode and QBackendNode subclasses when writing your own aspects.
 */

/*!
 * \typedef Qt3DCore::QPropertyValueAddedChangeBasePtr
 * \relates Qt3DCore::QPropertyValueAddedChangeBase
 *
 * A shared pointer for QPropertyValueAddedChangeBase.
 */

/*!
 * Constructs a new QPropertyValueAddedChangeBase with \a subjectId
 */
QPropertyValueAddedChangeBase::QPropertyValueAddedChangeBase(QNodeId subjectId)
    : QSceneChange(*new QPropertyValueAddedChangeBasePrivate, PropertyValueAdded, subjectId)
{
}

/*! \internal */
QPropertyValueAddedChangeBase::QPropertyValueAddedChangeBase(QPropertyValueAddedChangeBasePrivate &dd,
                                                             QNodeId subjectId)
    : QSceneChange(dd, PropertyValueAdded, subjectId)
{
}

QPropertyValueAddedChangeBase::~QPropertyValueAddedChangeBase()
{
}

} // namespace Qt3DCore

QT_END_NAMESPACE
