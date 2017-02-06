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

#include "qstenciloperationarguments.h"
#include "qstenciloperationarguments_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QStencilOperationArguments
    \brief The QStencilOperationArguments class sets the actions to be taken
    when stencil and depth tests fail.
    \since 5.7
    \ingroup renderstates
    \inmodule Qt3DRender

    The Qt3DRender::QStencilOperationArguments class specifies the arguments for
    the stencil operations.

    \sa Qt3DRender::QStencilOperation
 */

/*!
    \qmltype StencilOperationArguments
    \brief The StencilOperationArguments type sets the actions to be taken
    when stencil and depth tests fail.
    \since 5.7
    \ingroup renderstates
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QStencilOperationArguments
    \inherits QtObject

    The StencilOperationArguments type specifies the arguments for the stencil operations.

    \sa StencilOperation
 */

/*!
    \enum Qt3DRender::QStencilOperationArguments::FaceMode
    This enumeration holds the values for stencil operation argument face modes
    \value Front Arguments are applied to front-facing polygons.
    \value Back Arguments are applied to back-facing polygons.
    \value FrontAndBack Arguments are applied to both front- and back-facing polygons.
*/

/*!
    \enum Qt3DRender::QStencilOperationArguments::Operation
    This enumeration holds the values for stencil operation.
    \value Zero Set stencil value to zero.
    \value Keep Keep current stencil value.
    \value Replace Replace with the masked fragment stencil value.
    \value Increment Increment current value with saturation.
    \value Decrement Decrement current value with saturation.
    \value IncrementWrap Increment current value with wrap.
    \value DecrementWrap Decrement current value with wrap.
    \value Invert Invert the current value.
*/

/*!
    \qmlproperty enumeration StencilOperationArguments::faceMode
    Holds the faces the arguments are applied to.
    \list
    \li StencilOperationArguments.Front
    \li StencilOperationArguments.Back
    \li StencilOperationArguments.FrontAndBack
    \endlist
    \sa Qt3DRender::QStencilOperationArguments::FaceMode
    \readonly
*/

/*!
    \qmlproperty enumeration StencilOperationArguments::stencilTestFailureOperation
    Holds the stencil test operation for when the stencil test fails.
    Default is StencilOperationArguments.Keep.
    \list
    \li StencilOperationArguments.Zero
    \li StencilOperationArguments.Keep
    \li StencilOperationArguments.Replace
    \li StencilOperationArguments.Increment
    \li StencilOperationArguments.Decrement
    \li StencilOperationArguments.IncrementWrap
    \li StencilOperationArguments.DecrementWrap
    \li StencilOperationArguments.Inverter
    \endlist
    \sa Qt3DRender::QStencilOperationArguments::Operation
*/

/*!
    \qmlproperty enumeration StencilOperationArguments::depthTestFailureOperation
    Holds the stencil test operation for when the stencil test passes, but
    depth test fails. Default is StencilOperationArguments.Keep.
    \sa StencilOperationArguments::stencilTestFailureOperation, Qt3DRender::QStencilOperationArguments::Operation
*/

/*!
    \qmlproperty enumeration StencilOperationArguments::allTestsPassOperation
    Holds the stencil test operation for when depth and stencil test pass. Default is StencilOperationArguments.Keep.
    \sa StencilOperationArguments::stencilTestFailureOperation, Qt3DRender::QStencilOperationArguments::Operation
*/

/*!
    \property QStencilOperationArguments::faceMode
    Holds the faces the arguments are applied to.
    \readonly
*/

/*!
    \property QStencilOperationArguments::stencilTestFailureOperation
    Holds the stencil test operation for when the stencil test fails.
    Default is StencilOperationArguments.Keep.
*/

/*!
    \property QStencilOperationArguments::depthTestFailureOperation
    Holds the stencil test operation for when the stencil test passes, but
    depth test fails. Default is StencilOperationArguments.Keep.
*/

/*!
    \property QStencilOperationArguments::allTestsPassOperation
    Holds the stencil test operation for when depth and stencil test pass. Default is StencilOperationArguments.Keep.
*/

/*!
    The constructor creates a new QStencilOperationArguments::QStencilOperationArguments
    instance with the specified \a mode and \a parent.
 */
QStencilOperationArguments::QStencilOperationArguments(FaceMode mode, QObject *parent)
    : QObject(*new QStencilOperationArgumentsPrivate(mode), parent)
{
}

/*! \internal */
QStencilOperationArguments::~QStencilOperationArguments()
{
}

QStencilOperationArguments::FaceMode QStencilOperationArguments::faceMode() const
{
    Q_D(const QStencilOperationArguments);
    return d->m_face;
}

void QStencilOperationArguments::setStencilTestFailureOperation(QStencilOperationArguments::Operation operation)
{
    Q_D(QStencilOperationArguments);
    if (d->m_stencilTestFailureOperation != operation) {
        d->m_stencilTestFailureOperation = operation;
        Q_EMIT stencilTestFailureOperationChanged(operation);
    }
}

QStencilOperationArguments::Operation QStencilOperationArguments::stencilTestFailureOperation() const
{
    Q_D(const QStencilOperationArguments);
    return d->m_stencilTestFailureOperation;
}

void QStencilOperationArguments::setDepthTestFailureOperation(QStencilOperationArguments::Operation operation)
{
    Q_D(QStencilOperationArguments);
    if (d->m_depthTestFailureOperation != operation) {
        d->m_depthTestFailureOperation = operation;
        Q_EMIT depthTestFailureOperationChanged(operation);
    }
}

QStencilOperationArguments::Operation QStencilOperationArguments::depthTestFailureOperation() const
{
    Q_D(const QStencilOperationArguments);
    return d->m_depthTestFailureOperation;
}

void QStencilOperationArguments::setAllTestsPassOperation(QStencilOperationArguments::Operation operation)
{
    Q_D(QStencilOperationArguments);
    if (d->m_allTestsPassOperation != operation) {
        d->m_allTestsPassOperation = operation;
        Q_EMIT allTestsPassOperationChanged(operation);
    }
}

QStencilOperationArguments::Operation QStencilOperationArguments::allTestsPassOperation() const
{
    Q_D(const QStencilOperationArguments);
    return d->m_allTestsPassOperation;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
