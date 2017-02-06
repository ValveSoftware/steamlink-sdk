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

#include "qdeclarativerectanglemapitem_p.h"
#include "qdeclarativepolygonmapitem_p.h"
#include "qgeocameracapabilities_p.h"
#include "qlocationutils_p.h"
#include <QPainterPath>
#include <qnumeric.h>
#include <QRectF>
#include <QPointF>
#include <QtCore/QScopedValueRollback>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapRectangle
    \instantiates QDeclarativeRectangleMapItem
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.5

    \brief The MapRectangle type displays a rectangle on a Map.

    The MapRectangle type displays a rectangle on a Map. Rectangles are a
    special case of Polygon with exactly 4 vertices and 4 "straight" edges. In
    this case, "straight" means that the top-left point has the same latitude
    as the top-right point (the top edge), and the bottom-left point has the
    same latitude as the bottom-right point (the bottom edge). Similarly, the
    points on the left side have the same longitude, and the points on the
    right side have the same longitude.

    To specify the rectangle, it requires a \l topLeft and \l bottomRight point,
    both given by a \l {coordinate}.

    By default, the rectangle is displayed with transparent fill and a 1-pixel
    thick black border. This can be changed using the \l color, \l border.color
    and \l border.width properties.

    \note Similar to the \l MapPolygon type, MapRectangles are geographic
    items, thus dragging a MapRectangle causes its vertices to be recalculated
    in the geographic coordinate space. Apparent stretching of the item
    occurs when dragged to the a different latitude, however, its edges
    remain straight.

    \section2 Performance

    MapRectangles have a rendering cost identical to a MapPolygon with 4
    vertices.

    Like the other map objects, MapRectangle is normally drawn without a smooth
    appearance. Setting the \l opacity property will force the object to be
    blended, which decreases performance considerably depending on the hardware
    in use.

    \section2 Example Usage

    The following snippet shows a map containing a MapRectangle, spanning
    from (-27, 153) to (-28, 153.5), near Brisbane, Australia. The rectangle
    is filled in green, with a 2 pixel black border.

    \code
    Map {
        MapRectangle {
            color: 'green'
            border.width: 2
            topLeft {
                latitude: -27
                longitude: 153
            }
            bottomRight {
                latitude: -28
                longitude: 153.5
            }
        }
    }
    \endcode

    \image api-maprectangle.png
*/

struct Vertex
{
    QVector2D position;
};

QGeoMapRectangleGeometry::QGeoMapRectangleGeometry()
{
}

/*!
    \internal
*/
void QGeoMapRectangleGeometry::updatePoints(const QGeoMap &map,
                                            const QGeoCoordinate &topLeft,
                                            const QGeoCoordinate &bottomRight)
{
    if (!screenDirty_ && !sourceDirty_)
        return;

    QDoubleVector2D tl = map.coordinateToItemPosition(topLeft, false);
    QDoubleVector2D br = map.coordinateToItemPosition(bottomRight, false);

    // We can get NaN if the map isn't set up correctly, or the projection
    // is faulty -- probably best thing to do is abort
    if (!qIsFinite(tl.x()) || !qIsFinite(tl.y()))
        return;
    if (!qIsFinite(br.x()) || !qIsFinite(br.y()))
        return;

    if ( preserveGeometry_ ) {
        double unwrapBelowX = map.coordinateToItemPosition(geoLeftBound_, false).x();
        if (br.x() < unwrapBelowX)
            br.setX(tl.x() + screenBounds_.width());
    }

    QRectF re(tl.toPointF(), br.toPointF());
    re.translate(-1 * tl.toPointF());

    clear();
    screenVertices_.reserve(6);

    screenVertices_ << re.topLeft();
    screenVertices_ << re.topRight();
    screenVertices_ << re.bottomLeft();

    screenVertices_ << re.topRight();
    screenVertices_ << re.bottomLeft();
    screenVertices_ << re.bottomRight();

    firstPointOffset_ = QPointF(0,0);
    srcOrigin_ = topLeft;
    screenBounds_ = re;

    screenOutline_ = QPainterPath();
    screenOutline_.addRect(re);

    geoLeftBound_ = topLeft;
}

QDeclarativeRectangleMapItem::QDeclarativeRectangleMapItem(QQuickItem *parent)
:   QDeclarativeGeoMapItemBase(parent), color_(Qt::transparent), dirtyMaterial_(true),
    updatingGeometry_(false)
{
    setFlag(ItemHasContents, true);
    QObject::connect(&border_, SIGNAL(colorChanged(QColor)),
                     this, SLOT(markSourceDirtyAndUpdate()));
    QObject::connect(&border_, SIGNAL(widthChanged(qreal)),
                     this, SLOT(markSourceDirtyAndUpdate()));
}

QDeclarativeRectangleMapItem::~QDeclarativeRectangleMapItem()
{
}

/*!
    \internal
*/
void QDeclarativeRectangleMapItem::setMap(QDeclarativeGeoMap *quickMap, QGeoMap *map)
{
    QDeclarativeGeoMapItemBase::setMap(quickMap,map);
    if (map)
        markSourceDirtyAndUpdate();
}

/*!
    \qmlpropertygroup Location::MapRectangle::border
    \qmlproperty int MapRectangle::border.width
    \qmlproperty color MapRectangle::border.color

    This property is part of the border property group. The border property group
    holds the width and color used to draw the border of the rectangle.
    The width is in pixels and is independent of the zoom level of the map.

    The default values correspond to a black border with a width of 1 pixel.
    For no line, use a width of 0 or a transparent color.
*/
QDeclarativeMapLineProperties *QDeclarativeRectangleMapItem::border()
{
    return &border_;
}

/*!
    \qmlproperty coordinate MapRectangle::topLeft

    This property holds the top-left coordinate of the MapRectangle which
    can be used to retrieve its longitude, latitude and altitude.
*/
void QDeclarativeRectangleMapItem::setTopLeft(const QGeoCoordinate &topLeft)
{
    if (topLeft_ == topLeft)
        return;

    topLeft_ = topLeft;

    markSourceDirtyAndUpdate();
    emit topLeftChanged(topLeft_);
}

QGeoCoordinate QDeclarativeRectangleMapItem::topLeft()
{
    return topLeft_;
}

/*!
    \internal
*/
void QDeclarativeRectangleMapItem::markSourceDirtyAndUpdate()
{
    geometry_.markSourceDirty();
    borderGeometry_.markSourceDirty();
    polishAndUpdate();
}

/*!
    \qmlproperty coordinate MapRectangle::bottomRight

    This property holds the bottom-right coordinate of the MapRectangle which
    can be used to retrieve its longitude, latitude and altitude.
*/
void QDeclarativeRectangleMapItem::setBottomRight(const QGeoCoordinate &bottomRight)
{
    if (bottomRight_ == bottomRight)
        return;

    bottomRight_ = bottomRight;

    markSourceDirtyAndUpdate();
    emit bottomRightChanged(bottomRight_);
}

QGeoCoordinate QDeclarativeRectangleMapItem::bottomRight()
{
    return bottomRight_;
}

/*!
    \qmlproperty color MapRectangle::color

    This property holds the fill color of the rectangle. For no fill, use
    a transparent color.
*/
QColor QDeclarativeRectangleMapItem::color() const
{
    return color_;
}

void QDeclarativeRectangleMapItem::setColor(const QColor &color)
{
    if (color_ == color)
        return;
    color_ = color;
    dirtyMaterial_ = true;
    polishAndUpdate();
    emit colorChanged(color_);
}

/*!
  \qmlproperty real MapRectangle::opacity

  This property holds the opacity of the item.  Opacity is specified as a
  number between 0 (fully transparent) and 1 (fully opaque).  The default is 1.

  An item with 0 opacity will still receive mouse events. To stop mouse events, set the
  visible property of the item to false.
*/

/*!
    \internal
*/
QSGNode *QDeclarativeRectangleMapItem::updateMapItemPaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);

    MapPolygonNode *node = static_cast<MapPolygonNode *>(oldNode);

    if (!node) {
        node = new MapPolygonNode();
    }

    //TODO: update only material
    if (geometry_.isScreenDirty() || borderGeometry_.isScreenDirty() || dirtyMaterial_) {
        node->update(color_, border_.color(), &geometry_, &borderGeometry_);
        geometry_.setPreserveGeometry(false);
        borderGeometry_.setPreserveGeometry(false);
        geometry_.markClean();
        borderGeometry_.markClean();
        dirtyMaterial_ = false;
    }
    return node;
}

/*!
    \internal
*/
void QDeclarativeRectangleMapItem::updatePolish()
{
    if (!map() || !topLeft().isValid() || !bottomRight().isValid())
        return;

    QScopedValueRollback<bool> rollback(updatingGeometry_);
    updatingGeometry_ = true;

    geometry_.updatePoints(*map(), topLeft_, bottomRight_);

    QList<QGeoCoordinate> pathClosed;
    pathClosed << topLeft_;
    pathClosed << QGeoCoordinate(topLeft_.latitude(), bottomRight_.longitude());
    pathClosed << bottomRight_;
    pathClosed << QGeoCoordinate(bottomRight_.latitude(), topLeft_.longitude());
    pathClosed << pathClosed.first();

    if (border_.color() != Qt::transparent && border_.width() > 0) {
        borderGeometry_.updateSourcePoints(*map(), pathClosed, topLeft_);
        borderGeometry_.updateScreenPoints(*map(), border_.width());

        QList<QGeoMapItemGeometry *> geoms;
        geoms << &geometry_ << &borderGeometry_;
        QRectF combined = QGeoMapItemGeometry::translateToCommonOrigin(geoms);

        setWidth(combined.width());
        setHeight(combined.height());
    } else {
        borderGeometry_.clear();

        setWidth(geometry_.screenBoundingBox().width());
        setHeight(geometry_.screenBoundingBox().height());
    }

    setPositionOnMap(pathClosed.at(0), geometry_.firstPointOffset());
}

/*!
    \internal
*/
void QDeclarativeRectangleMapItem::afterViewportChanged(const QGeoMapViewportChangeEvent &event)
{
    if (event.mapSize.width() <= 0 || event.mapSize.height() <= 0)
        return;

    // if the scene is tilted, we must regenerate our geometry every frame
    if (map()->cameraCapabilities().supportsTilting()
            && (event.cameraData.tilt() > 0.1
                || event.cameraData.tilt() < -0.1)) {
        geometry_.markSourceDirty();
        borderGeometry_.markSourceDirty();
    }

    // if the scene is rolled, we must regen too
    if (map()->cameraCapabilities().supportsRolling()
            && (event.cameraData.roll() > 0.1
                || event.cameraData.roll() < -0.1)) {
        geometry_.markSourceDirty();
        borderGeometry_.markSourceDirty();
    }

    // otherwise, only regen on rotate, resize and zoom
    if (event.bearingChanged || event.mapSizeChanged || event.zoomLevelChanged) {
        geometry_.markSourceDirty();
        borderGeometry_.markSourceDirty();
    }
    geometry_.setPreserveGeometry(true, topLeft_);
    borderGeometry_.setPreserveGeometry(true, topLeft_);
    geometry_.markScreenDirty();
    borderGeometry_.markScreenDirty();
    polishAndUpdate();
}

/*!
    \internal
*/
bool QDeclarativeRectangleMapItem::contains(const QPointF &point) const
{
    return (geometry_.contains(point) || borderGeometry_.contains(point));
}

/*!
    \internal
*/
void QDeclarativeRectangleMapItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (updatingGeometry_ || newGeometry.topLeft() == oldGeometry.topLeft()) {
        QDeclarativeGeoMapItemBase::geometryChanged(newGeometry, oldGeometry);
        return;
    }

    QDoubleVector2D newTopLeftPoint = QDoubleVector2D(x(),y());
    QGeoCoordinate newTopLeft = map()->itemPositionToCoordinate(newTopLeftPoint, false);
    if (newTopLeft.isValid()) {
        // calculate new geo width while checking for dateline crossing
        const double lonW = bottomRight_.longitude() > topLeft_.longitude() ?
                    bottomRight_.longitude() - topLeft_.longitude() :
                    bottomRight_.longitude() + 360 - topLeft_.longitude();
        const double latH = qAbs(bottomRight_.latitude() - topLeft_.latitude());
        QGeoCoordinate newBottomRight;
        // prevent dragging over valid min and max latitudes
        if (QLocationUtils::isValidLat(newTopLeft.latitude() - latH)) {
            newBottomRight.setLatitude(newTopLeft.latitude() - latH);
        } else {
            newBottomRight.setLatitude(QLocationUtils::clipLat(newTopLeft.latitude() - latH));
            newTopLeft.setLatitude(newBottomRight.latitude() + latH);
        }
        // handle dateline crossing
        newBottomRight.setLongitude(QLocationUtils::wrapLong(newTopLeft.longitude() + lonW));
        newBottomRight.setAltitude(newTopLeft.altitude());
        topLeft_ = newTopLeft;
        bottomRight_ = newBottomRight;
        geometry_.setPreserveGeometry(true, newTopLeft);
        borderGeometry_.setPreserveGeometry(true, newTopLeft);
        markSourceDirtyAndUpdate();
        emit topLeftChanged(topLeft_);
        emit bottomRightChanged(bottomRight_);
    }

    // Not calling QDeclarativeGeoMapItemBase::geometryChanged() as it will be called from a nested
    // call to this function.
}

QT_END_NAMESPACE
