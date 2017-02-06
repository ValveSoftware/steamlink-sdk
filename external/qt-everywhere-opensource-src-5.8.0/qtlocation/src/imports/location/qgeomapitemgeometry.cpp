/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeomapitemgeometry_p.h"
#include "qdeclarativegeomap_p.h"
#include "qlocationutils_p.h"
#include <QtQuick/QSGGeometry>
#include "qdoublevector2d_p.h"

QT_BEGIN_NAMESPACE

QGeoMapItemGeometry::QGeoMapItemGeometry()
:   sourceDirty_(true), screenDirty_(true), clipToViewport_(true), preserveGeometry_(false)
{
}

/*!
    \internal
*/
void QGeoMapItemGeometry::translate(const QPointF &offset)
{
    for (int i = 0; i < screenVertices_.size(); ++i)
        screenVertices_[i] += offset;

    firstPointOffset_ += offset;
    screenOutline_.translate(offset);
    screenBounds_.translate(offset);
}

/*!
    \internal
*/
void QGeoMapItemGeometry::allocateAndFill(QSGGeometry *geom) const
{
    const QVector<QPointF> &vx = screenVertices_;
    const QVector<quint32> &ix = screenIndices_;

    if (isIndexed()) {
        geom->allocate(vx.size(), ix.size());
        if (geom->indexType() == QSGGeometry::UnsignedShortType) {
            quint16 *its = geom->indexDataAsUShort();
            for (int i = 0; i < ix.size(); ++i)
                its[i] = ix[i];
        } else if (geom->indexType() == QSGGeometry::UnsignedIntType) {
            quint32 *its = geom->indexDataAsUInt();
            for (int i = 0; i < ix.size(); ++i)
                its[i] = ix[i];
        }
    } else {
        geom->allocate(vx.size());
    }

    QSGGeometry::Point2D *pts = geom->vertexDataAsPoint2D();
    for (int i = 0; i < vx.size(); ++i)
        pts[i].set(vx[i].x(), vx[i].y());
}

/*!
    \internal
*/
QRectF QGeoMapItemGeometry::translateToCommonOrigin(const QList<QGeoMapItemGeometry *> &geoms)
{
    QGeoCoordinate origin = geoms.at(0)->origin();

    QPainterPath brects;

    // first get max offset
    QPointF maxOffset = geoms.at(0)->firstPointOffset();
    foreach (QGeoMapItemGeometry *g, geoms) {
        Q_ASSERT(g->origin() == origin);
        QPointF o = g->firstPointOffset();
        maxOffset.setX(qMax(o.x(), maxOffset.x()));
        maxOffset.setY(qMax(o.y(), maxOffset.y()));
    }

    // then translate everything
    foreach (QGeoMapItemGeometry *g, geoms) {
        g->translate(maxOffset - g->firstPointOffset());
        brects.addRect(g->sourceBoundingBox());
    }

    return brects.boundingRect();
}

/*!
    \internal
*/
double QGeoMapItemGeometry::geoDistanceToScreenWidth(const QGeoMap &map,
                                                     const QGeoCoordinate &fromCoord,
                                                     const QGeoCoordinate &toCoord)
{
    // Do not wrap around half the globe
    Q_ASSERT(!qFuzzyCompare(fromCoord.longitude(), toCoord.longitude()));

    QGeoCoordinate mapMid = map.itemPositionToCoordinate(QDoubleVector2D(map.viewportWidth()/2.0, 0));
    double halfGeoDist = toCoord.longitude() - fromCoord.longitude();
    if (toCoord.longitude() < fromCoord.longitude())
        halfGeoDist += 360;
    halfGeoDist /= 2.0;
    QGeoCoordinate geoDelta =  QGeoCoordinate(0,
                    QLocationUtils::wrapLong(mapMid.longitude() + halfGeoDist));
    QDoubleVector2D halfScreenDist = map.coordinateToItemPosition(geoDelta, false)
                                - QDoubleVector2D(map.viewportWidth()/2.0, 0);
    return halfScreenDist.x() * 2.0;
}

QT_END_NAMESPACE
