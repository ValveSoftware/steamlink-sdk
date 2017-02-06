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

#include "qpointsize.h"
#include "qpointsize_p.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QPointSize
    \inmodule Qt3DRender
    \since 5.7
    \brief Specifies the size of rasterized points. May either be set statically
    or by shader programs.

    When the sizeMode property is set to SizeMode::Fixed, the value is set
    using glPointSize(), if available. When using SizeMode::Programmable,
    gl_PointSize must be set within shader programs, the value provided to this
    RenderState is ignored in that case.
 */

/*!
    \qmltype PointSize
    \since 5.7
    \inherits RenderState
    \instantiates Qt3DRender::QPointSize
    \inqmlmodule Qt3D.Render

    \brief Specifies the size of rasterized points. May either be set statically
    or by shader programs.

    When the sizeMode property is set to SizeMode::Fixed, the value is set
    using glPointSize(), if available. When using SizeMode::Programmable,
    gl_PointSize must be set within shader programs, the value provided to this
    RenderState is ignored in that case.
 */

/*!
    \enum Qt3DRender::QPointSize::SizeMode

    This enumeration specifies values for the size mode.
    \value Fixed The point size is by the QPointSize::value.
    \value Programmable The point size value must be set in shader
*/
/*!
    \qmlproperty real PointSize::value
    Specifies the point size value to be used.
*/

/*!
    \qmlproperty enumeration PointSize::sizeMode
    Specifies the sizeMode to be used.
*/

/*!
    \property  QPointSize::value
    Specifies the point size value to be used.
*/

/*!
    \property QPointSize::sizeMode
    Specifies the sizeMode to be used.
*/

QPointSize::QPointSize(Qt3DCore::QNode *parent)
    : QRenderState(*new QPointSizePrivate(SizeMode::Programmable, 0.f), parent)
{
}

/*! \internal */
QPointSize::~QPointSize()
{
}

QPointSize::SizeMode QPointSize::sizeMode() const
{
    Q_D(const QPointSize);
    return d->m_sizeMode;
}

float QPointSize::value() const
{
    Q_D(const QPointSize);
    return d->m_value;
}

void QPointSize::setSizeMode(SizeMode sizeMode)
{
    Q_D(QPointSize);
    d->m_sizeMode = sizeMode;
    emit sizeModeChanged(sizeMode);
}

void QPointSize::setValue(float size)
{
    Q_D(QPointSize);
    d->m_value = size;
    emit valueChanged(size);
}

Qt3DCore::QNodeCreatedChangeBasePtr QPointSize::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QPointSizeData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QPointSize);
    data.sizeMode = d->m_sizeMode;
    data.value = d->m_value;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE

