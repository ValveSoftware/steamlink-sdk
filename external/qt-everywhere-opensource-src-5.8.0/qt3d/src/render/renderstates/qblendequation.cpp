/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

/*!
 * \class Qt3DRender::QBlendEquation
 * \brief The QBlendEquation class specifies the equation used for both the RGB
 *  blend equation and the Alpha blend equation
 * \inmodule Qt3DRender
 * \since 5.7
 * \ingroup renderstates
 *
 * The blend equation is used to determine how a new pixel is combined with a pixel
 * already in the framebuffer.
 */

/*!
    \qmltype BlendEquation
    \instantiates Qt3DRender::QBlendEquation
    \inherits RenderState
    \inqmlmodule Qt3D.Render
    \since 5.5
    \brief The BlendEquation class specifies the equation used for both the RGB
     blend equation and the Alpha blend equation

    The blend equation is used to determine how a new pixel is combined with a pixel
    already in the framebuffer.
*/

#include "qblendequation.h"
#include "qblendequation_p.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
 * The constructor creates a new blend state object with the specified \a parent.
 */
QBlendEquation::QBlendEquation(QNode *parent)
    : QRenderState(*new QBlendEquationPrivate, parent)
{
}

/*! \internal */
QBlendEquation::~QBlendEquation()
{
}

/*!
  \enum Qt3DRender::QBlendEquation::BlendFunction

  \value Add GL_FUNC_ADD
  \value Subtract GL_FUNC_SUBTRACT
  \value ReverseSubtract GL_FUNC_REVERSE_SUBTRACT
  \value Min GL_MIN
  \value Max GL_MAX
*/

/*!
    \qmlproperty enumeration BlendEquation::blendFunction

    Holds the blend function, which determines how source and destination colors are combined.
 */

/*!
    \property QBlendEquation::blendFunction

    Holds the blend function, which determines how source and destination colors are combined.
 */

QBlendEquation::BlendFunction QBlendEquation::blendFunction() const
{
    Q_D(const QBlendEquation);
    return d->m_blendFunction;
}

void QBlendEquation::setBlendFunction(QBlendEquation::BlendFunction blendFunction)
{
    Q_D(QBlendEquation);
    if (d->m_blendFunction != blendFunction) {
        d->m_blendFunction = blendFunction;
        emit blendFunctionChanged(blendFunction);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QBlendEquation::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QBlendEquationData>::create(this);
    auto &data = creationChange->data;
    data.blendFunction = d_func()->m_blendFunction;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
