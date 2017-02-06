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

#include "qabstractfunctor.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QAbstractFunctor
    \inmodule Qt3DRender
    \since 5.7
    \brief QAbstractFunctor is an abstract base class for all functors.

    The QAbstractFunctor is used as a base class for all functors and data
    generators in Qt3DRender module.

    When user defines a new functor or generator, they need to implement the
    \l QAbstractFunctor::id() method, which should be done using the \c {QT3D_FUNCTOR}
    macro in the class definition.
 */
/*!
    \fn qintptr QAbstractFunctor::id() const
 */
/*!
    \macro QT3D_FUNCTOR(Class)
    \relates Qt3DRender::QAbstractFunctor

    This macro assigns functor id to the \a Class, which is used by QAbstractFunctor::functor_cast
    to determine if the cast can be done.
 */

/*!
    \fn const T *QAbstractFunctor::functor_cast(const QAbstractFunctor *other) const

    This method is used to cast functor \a other to type T if the other is of
    type T (or of subclass); otherwise returns 0. This method works similarly
    to \l QObject::qobject_cast, except with functors derived from QAbstractFunctor.

    \warning If T was not declared with \l QT3D_FUNCTOR macro, then the results are undefined.
  */

/*! Desctructor */
QAbstractFunctor::~QAbstractFunctor()
{

}

} // Qt3D

QT_END_NAMESPACE

