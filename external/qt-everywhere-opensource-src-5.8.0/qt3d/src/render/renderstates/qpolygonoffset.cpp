/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qpolygonoffset.h"
#include "qpolygonoffset_p.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QPolygonOffset
    \brief The QPolygonOffset class sets the scale and steps to calculate depth
    values for polygon offsets.
    \since 5.7
    \inmodule Qt3DRender
    \ingroup renderstates

    A QPolygonOffset class adds an offset to the fragment depth value prior to
    depth test and depth write. The offset can be used to avoid z-fighting when
    rendering polygons with very close depth values such as decals.
 */

/*!
    \qmltype PolygonOffset
    \brief The PolygonOffset type sets the scale and steps to calculate depth
    values for polygon offsets.
    \since 5.7
    \inqmlmodule Qt3D.Render
    \ingroup renderstates
    \inherits RenderState
    \instantiates Qt3DRender::QPolygonOffset

    A PolygonOffset type adds an offset to the fragment depth value prior to
    depth test and depth write. The offset can be used to avoid z-fighting when
    rendering polygons with very close depth values such as decals.
 */

/*!
    \qmlproperty real PolygonOffset::scaleFactor
    Holds the scale factor used to create a variable depth offset for
    each polygon. Default value is 0.
*/

/*!
    \qmlproperty real PolygonOffset::depthSteps
    Holds the units that create constant depth offsets. Default value is 0.
*/

/*!
    \property QPolygonOffset::scaleFactor
    Holds the scale factor used to create a variable depth offset for
    each polygon. Default value is 0.
*/

/*!
    \property QPolygonOffset::depthSteps
    Holds the units that create constant depth offsets. Default value is 0.
*/

/*!
    The constructor creates a new QPolygonOffset::QPolygonOffset instance
    with the specified \a parent
 */
QPolygonOffset::QPolygonOffset(QNode *parent)
    : QRenderState(*new QPolygonOffsetPrivate, parent)
{
}

/*! \internal */
QPolygonOffset::~QPolygonOffset()
{
}

float QPolygonOffset::scaleFactor() const
{
    Q_D(const QPolygonOffset);
    return d->m_scaleFactor;
}

void QPolygonOffset::setScaleFactor(float scaleFactor)
{
    Q_D(QPolygonOffset);
    if (d->m_scaleFactor != scaleFactor) {
        d->m_scaleFactor = scaleFactor;
        emit scaleFactorChanged(d->m_scaleFactor);
    }
}

float QPolygonOffset::depthSteps() const
{
    Q_D(const QPolygonOffset);
    return d->m_depthSteps;
}

void QPolygonOffset::setDepthSteps(float depthSteps)
{
    Q_D(QPolygonOffset);
    if (d->m_depthSteps != depthSteps) {
        d->m_depthSteps = depthSteps;
        emit depthStepsChanged(d->m_depthSteps);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QPolygonOffset::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QPolygonOffsetData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QPolygonOffset);
    data.scaleFactor = d->m_scaleFactor;
    data.depthSteps = d->m_depthSteps;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
