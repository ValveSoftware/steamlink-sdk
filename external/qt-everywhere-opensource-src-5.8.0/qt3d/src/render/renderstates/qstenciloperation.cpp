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

#include "qstenciloperation.h"
#include "qstenciloperation_p.h"
#include "qstenciloperationarguments.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QStencilOperation
    \brief The QStencilOperation class specifies stencil operation
    \since 5.7
    \ingroup renderstates
    \inmodule Qt3DRender

    A Qt3DRender::QStencilOperation class specifies the stencil operations
    for the front- and back-facing polygons. The stencil operation control
    what is done to fragment when the stencil and depth test pass or fail.

    \sa Qt3DRender::QStencilTest
 */

/*!
    \qmltype StencilOperation
    \brief The StencilOperation type specifies stencil operation
    \since 5.7
    \ingroup renderstates
    \inqmlmodule Qt3D.Render
    \inherits RenderState
    \instantiates Qt3DRender::QStencilOperation

    A StencilOperation type specifies the stencil operations
    for the front- and back-facing polygons. The stencil operation control
    what is done to fragment when the stencil and depth test pass or fail.

    \sa StencilTest
 */

/*!
    \qmlproperty StencilOperationArguments StencilOperation::front
    Holds the stencil operation arguments for front-facing polygons.
*/

/*!
    \qmlproperty StencilOperationArguments StencilOperation::back
    Holds the stencil operation arguments for back-facing polygons.
*/

/*!
    \property QStencilOperation::front
    Holds the stencil operation arguments for front-facing polygons.
*/

/*!
    \property QStencilOperation::back
    Holds the stencil operation arguments for back-facing polygons.
*/

/*!
    The constructor creates a new QStencilOperation::QStencilOperation instance with
    the specified \a parent.
 */
QStencilOperation::QStencilOperation(QNode *parent)
    : QRenderState(*new QStencilOperationPrivate(), parent)
{
}

/*! \internal */
QStencilOperation::~QStencilOperation()
{
}

QStencilOperationArguments *QStencilOperation::front() const
{
    Q_D(const QStencilOperation);
    return d->m_front;
}

QStencilOperationArguments *QStencilOperation::back() const
{
    Q_D(const QStencilOperation);
    return d->m_back;
}

Qt3DCore::QNodeCreatedChangeBasePtr QStencilOperation::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QStencilOperationData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QStencilOperation);
    data.front.face = d->m_front->faceMode();
    data.front.stencilTestFailureOperation = d->m_front->stencilTestFailureOperation();
    data.front.depthTestFailureOperation = d->m_front->depthTestFailureOperation();
    data.front.allTestsPassOperation = d->m_front->allTestsPassOperation();
    data.back.face = d->m_back->faceMode();
    data.back.stencilTestFailureOperation = d->m_back->stencilTestFailureOperation();
    data.back.depthTestFailureOperation = d->m_back->depthTestFailureOperation();
    data.back.allTestsPassOperation = d->m_back->allTestsPassOperation();
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
