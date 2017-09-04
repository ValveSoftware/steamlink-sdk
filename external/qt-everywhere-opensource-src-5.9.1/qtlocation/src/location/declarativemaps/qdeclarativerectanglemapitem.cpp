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
#include "qlocationutils_p.h"
#include <QPainterPath>
#include <qnumeric.h>
#include <QRectF>
#include <QPointF>
#include <QtLocation/private/qgeomap_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>
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

QDeclarativeRectangleMapItem::QDeclarativeRectangleMapItem(QQuickItem *parent)
:   QDeclarativeGeoMapItemBase(parent), border_(this), color_(Qt::transparent), dirtyMaterial_(true),
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
    if (!map)
        return;
    updatePath();
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
    if (rectangle_.topLeft() == topLeft)
        return;

    rectangle_.setTopLeft(topLeft);
    updatePath();
    markSourceDirtyAndUpdate();
    emit topLeftChanged(topLeft);
}

QGeoCoordinate QDeclarativeRectangleMapItem::topLeft()
{
    return rectangle_.topLeft();
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
    if (rectangle_.bottomRight() == bottomRight)
        return;

    rectangle_.setBottomRight(bottomRight);
    updatePath();
    markSourceDirtyAndUpdate();
    emit bottomRightChanged(bottomRight);
}

QGeoCoordinate QDeclarativeRectangleMapItem::bottomRight()
{
    return rectangle_.bottomRight();
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

    geometry_.setPreserveGeometry(true, rectangle_.topLeft());
    geometry_.updateSourcePoints(*map(), pathMercator_);
    geometry_.updateScreenPoints(*map());

    QList<QGeoMapItemGeometry *> geoms;
    geoms << &geometry_;
    borderGeometry_.clear();

    if (border_.color() != Qt::transparent && border_.width() > 0) {
        QList<QDoubleVector2D> closedPath = pathMercator_;
        closedPath << closedPath.first();

        borderGeometry_.setPreserveGeometry(true, rectangle_.topLeft());
        const QGeoCoordinate &geometryOrigin = geometry_.origin();

        borderGeometry_.srcPoints_.clear();
        borderGeometry_.srcPointTypes_.clear();

        QDoubleVector2D borderLeftBoundWrapped;
        QList<QList<QDoubleVector2D > > clippedPaths = borderGeometry_.clipPath(*map(), closedPath, borderLeftBoundWrapped);
        if (clippedPaths.size()) {
            borderLeftBoundWrapped = map()->geoProjection().geoToWrappedMapProjection(geometryOrigin);
            borderGeometry_.pathToScreen(*map(), clippedPaths, borderLeftBoundWrapped);
            borderGeometry_.updateScreenPoints(*map(), border_.width());

            geoms << &borderGeometry_;
        } else {
            borderGeometry_.clear();
        }
    }

    QRectF combined = QGeoMapItemGeometry::translateToCommonOrigin(geoms);
    setWidth(combined.width());
    setHeight(combined.height());

    setPositionOnMap(geometry_.origin(), geometry_.firstPointOffset());
}

/*!
    \internal
*/
void QDeclarativeRectangleMapItem::afterViewportChanged(const QGeoMapViewportChangeEvent &event)
{
    if (event.mapSize.width() <= 0 || event.mapSize.height() <= 0)
        return;

    geometry_.setPreserveGeometry(true, rectangle_.topLeft());
    borderGeometry_.setPreserveGeometry(true, rectangle_.topLeft());
    markSourceDirtyAndUpdate();
}

/*!
    \internal
*/
bool QDeclarativeRectangleMapItem::contains(const QPointF &point) const
{
    return (geometry_.contains(point) || borderGeometry_.contains(point));
}

const QGeoShape &QDeclarativeRectangleMapItem::geoShape() const
{
    return rectangle_;
}

QGeoMap::ItemType QDeclarativeRectangleMapItem::itemType() const
{
    return QGeoMap::MapRectangle;
}

/*!
    \internal
*/
void QDeclarativeRectangleMapItem::updatePath()
{
    if (!map())
        return;
    pathMercator_.clear();
    pathMercator_ << map()->geoProjection().geoToMapProjection(rectangle_.topLeft());
    pathMercator_ << map()->geoProjection().geoToMapProjection(
                         QGeoCoordinate(rectangle_.topLeft().latitude(), rectangle_.bottomRight().longitude()));
    pathMercator_ << map()->geoProjection().geoToMapProjection(rectangle_.bottomRight());
    pathMercator_ << map()->geoProjection().geoToMapProjection(
                         QGeoCoordinate(rectangle_.bottomRight().latitude(), rectangle_.topLeft().longitude()));
}

/*!
    \internal
*/
void QDeclarativeRectangleMapItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (!map() || !rectangle_.isValid() || updatingGeometry_ || newGeometry.topLeft() == oldGeometry.topLeft()) {
        QDeclarativeGeoMapItemBase::geometryChanged(newGeometry, oldGeometry);
        return;
    }
    // TODO: change the algorithm to preserve the distances and size
    QGeoCoordinate newCenter = map()->geoProjection().itemPositionToCoordinate(QDoubleVector2D(newGeometry.center()), false);
    QGeoCoordinate oldCenter = map()->geoProjection().itemPositionToCoordinate(QDoubleVector2D(oldGeometry.center()), false);
    if (!newCenter.isValid() || !oldCenter.isValid())
        return;
    double offsetLongi = newCenter.longitude() - oldCenter.longitude();
    double offsetLati = newCenter.latitude() - oldCenter.latitude();
    if (offsetLati == 0.0 && offsetLongi == 0.0)
        return;

    rectangle_.translate(offsetLati, offsetLongi);
    updatePath();
    geometry_.setPreserveGeometry(true, rectangle_.topLeft());
    borderGeometry_.setPreserveGeometry(true, rectangle_.topLeft());
    markSourceDirtyAndUpdate();
    emit topLeftChanged(rectangle_.topLeft());
    emit bottomRightChanged(rectangle_.bottomRight());

    // Not calling QDeclarativeGeoMapItemBase::geometryChanged() as it will be called from a nested
    // call to this function.
}

QT_END_NAMESPACE
