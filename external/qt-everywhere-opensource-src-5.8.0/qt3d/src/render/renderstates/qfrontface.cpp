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

#include "qfrontface.h"
#include "qfrontface_p.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QFrontFace
    \brief The QFrontFace class defines front and back facing polygons.
    \since 5.7
    \ingroup renderstates
    \inmodule Qt3DRender

    A Qt3DRender::QFrontFace sets the winding direction of the front facing polygons.

    \sa QCullFace
 */

/*!
    \qmltype FrontFace
    \brief The FrontFace type defines front and back facing polygons.
    \since 5.7
    \ingroup renderstates
    \inqmlmodule Qt3D.Render
    \inherits RenderState
    \instantiates Qt3DRender::QFrontFace

    A FrontFace sets the winding direction of the front facing polygons.

    \sa CullFace
 */

/*!
    \enum QFrontFace::WindingDirection

    This enumeration specifies the winding direction values.
    \value ClockWise Clockwise polygons are front facing.
    \value CounterClockWise Counter clockwise polygons are front facing.
*/

/*!
    \qmlproperty enumeration FrontFace::direction
    Holds the winding direction of the front facing polygons. Default is FrontFace.Clockwise.
*/

/*!
    \property QFrontFace::direction
    Holds the winding direction of the front facing polygons. Default is Clockwise.
*/

/*!
    The constructor creates a new QFrontFace::QFrontFace instance with the
    specified \a parent
 */
QFrontFace::QFrontFace(QNode *parent)
    : QRenderState(*new QFrontFacePrivate, parent)
{
}

/*! \internal */
QFrontFace::~QFrontFace()
{
}

QFrontFace::WindingDirection QFrontFace::direction() const
{
    Q_D(const QFrontFace);
    return d->m_direction;
}

void QFrontFace::setDirection(QFrontFace::WindingDirection direction)
{
    Q_D(QFrontFace);
    if (d->m_direction != direction) {
        d->m_direction = direction;
        emit directionChanged(direction);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QFrontFace::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QFrontFaceData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QFrontFace);
    data.direction = d->m_direction;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
