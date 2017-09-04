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

#include "qviewport.h"
#include "qviewport_p.h"

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DRender/qframegraphnodecreatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

QViewportPrivate::QViewportPrivate()
    : QFrameGraphNodePrivate()
    , m_normalizedRect(QRectF(0.0f, 0.0f, 1.0f, 1.0f))
    , m_gamma(2.2f)
{
}

/*!
    \class Qt3DRender::QViewport
    \inmodule Qt3DRender
    \brief A viewport on the Qt3D Scene
    \since 5.7

    \inherits Qt3DRender::QFrameGraphNode

    Qt3DRender::QViewport of the scene specifies at which portion of the render surface Qt3D
    is rendering to. Area outside the viewport is left untouched. It also controls global parameters
    to the rendering in that viewport like gamma.
 */

/*!
    \qmltype Viewport
    \inqmlmodule Qt3D.Render
    \since 5.7
    \inherits FrameGraphNode
    \instantiates Qt3DRender::QViewport
    \brief A viewport on the Qt3D Scene

    Viewport of the scene specifies at which portion of the render surface Qt3D is
    rendering to. Area outside the viewport is left untouched. It also controls global parameters
    to the rendering in that viewport like gamma.
 */

/*!
    \qmlproperty rect Viewport::normalizedRect

    Specifies the normalised rectangle for the viewport, i.e. the viewport rectangle
    is specified relative to the render surface size. Whole surface sized viewport
    is specified as [0.0, 0.0, 1.0, 1.0], which is the default.
 */

/*!
    \qmlproperty rect Viewport::gamma

    Specifies the gamma factor for the viewport. The default is 2.2 which should give proper result on most screens.
 */

/*!
    Constructs QViewport with given \a parent.
 */
QViewport::QViewport(QNode *parent)
    : QFrameGraphNode(*new QViewportPrivate, parent)
{
}

/*! \internal */
QViewport::~QViewport()
{
}

/*! \internal */
QViewport::QViewport(QViewportPrivate &dd, QNode *parent)
    : QFrameGraphNode(dd, parent)
{
}


QRectF QViewport::normalizedRect() const
{
    Q_D(const QViewport);
    return d->m_normalizedRect;
}

float QViewport::gamma() const
{
    Q_D(const QViewport);
    return d->m_gamma;
}

/*!
    \property QViewport::normalizedRect

    Specifies the normalised rectangle for the viewport, i.e. the viewport rectangle
    is specified relative to the render surface size. Whole surface sized viewport
    is specified as [0.0, 0.0, 1.0, 1.0], which is the default.
 */
void QViewport::setNormalizedRect(const QRectF &normalizedRect)
{
    Q_D(QViewport);
    if (normalizedRect != d->m_normalizedRect) {
        d->m_normalizedRect = normalizedRect;
        emit normalizedRectChanged(normalizedRect);
    }
}

/*!
    \property QViewport::gamma

    Specifies the gamma factor for the viewport. The default is 2.2 which should give proper result on most screens.
 */
void QViewport::setGamma(float gamma)
{
    Q_D(QViewport);
    if (gamma != d->m_gamma) {
        d->m_gamma = gamma;
        emit gammaChanged(gamma);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QViewport::createNodeCreationChange() const
{
    auto creationChange = QFrameGraphNodeCreatedChangePtr<QViewportData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QViewport);
    data.normalizedRect = d->m_normalizedRect;
    data.gamma = d->m_gamma;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
