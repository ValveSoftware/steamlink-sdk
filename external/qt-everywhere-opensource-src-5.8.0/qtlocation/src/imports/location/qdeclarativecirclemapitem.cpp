/***************************************************************************
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

#include "qdeclarativecirclemapitem_p.h"
#include "qdeclarativepolygonmapitem_p.h"
#include "qgeocameracapabilities_p.h"
#include "qgeoprojection_p.h"

#include <cmath>

#include <QtCore/QScopedValueRollback>
#include <QPen>
#include <QPainter>

#include "qdoublevector2d_p.h"

/* poly2tri triangulator includes */
#include "../../3rdparty/poly2tri/common/shapes.h"
#include "../../3rdparty/poly2tri/sweep/cdt.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapCircle
    \instantiates QDeclarativeCircleMapItem
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.5

    \brief The MapCircle type displays a geographic circle on a Map.

    The MapCircle type displays a geographic circle on a Map, which
    consists of all points that are within a set distance from one
    central point. Depending on map projection, a geographic circle
    may not always be a perfect circle on the screen: for instance, in
    the Mercator projection, circles become ovoid in shape as they near
    the poles. To display a perfect screen circle around a point, use a
    MapQuickItem containing a relevant Qt Quick type instead.

    By default, the circle is displayed as a 1 pixel black border with
    no fill. To change its appearance, use the color, border.color
    and border.width properties.

    Internally, a MapCircle is implemented as a many-sided polygon. To
    calculate the radius points it uses a spherical model of the Earth,
    similar to the atDistanceAndAzimuth method of the \l {coordinate}
    type. These two things can occasionally have implications for the
    accuracy of the circle's shape, depending on position and map
    projection.

    \note Dragging a MapCircle (through the use of \l MouseArea)
    causes new points to be generated at the same distance (in meters)
    from the center. This is in contrast to other map items which store
    their dimensions in terms of latitude and longitude differences between
    vertices.

    \section2 Performance

    MapCircle performance is almost equivalent to that of a MapPolygon with
    the same number of vertices. There is a small amount of additional
    overhead with respect to calculating the vertices first.

    Like the other map objects, MapCircle is normally drawn without a smooth
    appearance. Setting the opacity property will force the object to be
    blended, which decreases performance considerably depending on the graphics
    hardware in use.

    \section2 Example Usage

    The following snippet shows a map containing a MapCircle, centered at
    the coordinate (-27, 153) with a radius of 5km. The circle is
    filled in green, with a 3 pixel black border.

    \code
    Map {
        MapCircle {
            center {
                latitude: -27.5
                longitude: 153.0
            }
            radius: 5000.0
            color: 'green'
            border.width: 3
        }
    }
    \endcode

    \image api-mapcircle.png
*/

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const int CircleSamples = 128;

struct Vertex
{
    QVector2D position;
};

QGeoMapCircleGeometry::QGeoMapCircleGeometry()
{
}

/*!
    \internal
*/
void QGeoMapCircleGeometry::updateScreenPointsInvert(const QGeoMap &map)
{
    if (!screenDirty_)
        return;

    if (map.viewportWidth() == 0 || map.viewportHeight() == 0) {
        clear();
        return;
    }

    QPointF origin = map.coordinateToItemPosition(srcOrigin_, false).toPointF();

    QPainterPath ppi = srcPath_;

    clear();

    // a circle requires at least 3 points;
    if (ppi.elementCount() < 3)
        return;

    // translate the path into top-left-centric coordinates
    QRectF bb = ppi.boundingRect();
    ppi.translate(-bb.left(), -bb.top());
    firstPointOffset_ = -1 * bb.topLeft();

    ppi.closeSubpath();

    // calculate actual width of map on screen in pixels
    QGeoCoordinate mapCenter(0, map.cameraData().center().longitude());
    QDoubleVector2D midPoint = map.coordinateToItemPosition(mapCenter, false);
    QDoubleVector2D midPointPlusOne = QDoubleVector2D(midPoint.x() + 1.0, midPoint.y());
    QGeoCoordinate coord1 = map.itemPositionToCoordinate(midPointPlusOne, false);
    double geoDistance = coord1.longitude() - map.cameraData().center().longitude();
    if ( geoDistance < 0 )
        geoDistance += 360.0;
    double mapWidth = 360.0 / geoDistance;

    qreal leftOffset = origin.x() - (map.viewportWidth()/2.0 - mapWidth/2.0) - firstPointOffset_.x();
    qreal topOffset = origin.y() - (midPoint.y() - mapWidth/2.0) - firstPointOffset_.y();
    QPainterPath ppiBorder;
    ppiBorder.moveTo(QPointF(-leftOffset, -topOffset));
    ppiBorder.lineTo(QPointF(mapWidth - leftOffset, -topOffset));
    ppiBorder.lineTo(QPointF(mapWidth - leftOffset, mapWidth - topOffset));
    ppiBorder.lineTo(QPointF(-leftOffset, mapWidth - topOffset));

    screenOutline_ = ppiBorder;

    std::vector<p2t::Point*> borderPts;
    borderPts.reserve(4);

    std::vector<p2t::Point*> curPts;
    curPts.reserve(ppi.elementCount());
    for (int i = 0; i < ppi.elementCount(); ++i) {
        const QPainterPath::Element e = ppi.elementAt(i);
        if (e.isMoveTo() || i == ppi.elementCount() - 1
                || (qAbs(e.x - curPts.front()->x) < 0.1
                    && qAbs(e.y - curPts.front()->y) < 0.1)) {
            if (curPts.size() > 2) {
                for (int j = 0; j < 4; ++j) {
                    const QPainterPath::Element e2 = ppiBorder.elementAt(j);
                    borderPts.push_back(new p2t::Point(e2.x, e2.y));
                }
                p2t::CDT *cdt = new p2t::CDT(borderPts);
                cdt->AddHole(curPts);
                cdt->Triangulate();
                std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
                screenVertices_.reserve(screenVertices_.size() + int(tris.size()));
                for (size_t i = 0; i < tris.size(); ++i) {
                    p2t::Triangle *t = tris.at(i);
                    for (int j = 0; j < 3; ++j) {
                        p2t::Point *p = t->GetPoint(j);
                        screenVertices_ << QPointF(p->x, p->y);
                    }
                }
                delete cdt;
            }
            curPts.clear();
            curPts.reserve(ppi.elementCount() - i);
            curPts.push_back(new p2t::Point(e.x, e.y));
        } else if (e.isLineTo()) {
            curPts.push_back(new p2t::Point(e.x, e.y));
        } else {
            qWarning("Unhandled element type in circle painterpath");
        }
    }

    if (curPts.size() > 0) {
        qDeleteAll(curPts.begin(), curPts.end());
        curPts.clear();
    }

    if (borderPts.size() > 0) {
        qDeleteAll(borderPts.begin(), borderPts.end());
        borderPts.clear();
    }

    screenBounds_ = ppiBorder.boundingRect();

}

static const qreal qgeocoordinate_EARTH_MEAN_RADIUS = 6371.0072;

inline static qreal qgeocoordinate_degToRad(qreal deg)
{
    return deg * M_PI / 180;
}
inline static qreal qgeocoordinate_radToDeg(qreal rad)
{
    return rad * 180 / M_PI;
}

static bool crossEarthPole(const QGeoCoordinate &center, qreal distance)
{
    qreal poleLat = 90;
    QGeoCoordinate northPole = QGeoCoordinate(poleLat, center.longitude());
    QGeoCoordinate southPole = QGeoCoordinate(-poleLat, center.longitude());
    // approximate using great circle distance
    qreal distanceToNorthPole = center.distanceTo(northPole);
    qreal distanceToSouthPole = center.distanceTo(southPole);
    if (distanceToNorthPole < distance || distanceToSouthPole < distance)
        return true;
    return false;
}

static void calculatePeripheralPoints(QList<QGeoCoordinate> &path,
                                      const QGeoCoordinate &center,
                                      qreal distance,
                                      int steps,
                                      QGeoCoordinate &leftBound )
{
    // Calculate points based on great-circle distance
    // Calculation is the same as GeoCoordinate's atDistanceAndAzimuth function
    // but tweaked here for computing multiple points

    // pre-calculations
    steps = qMax(steps, 3);
    qreal centerLon = center.longitude();
    qreal minLon = centerLon;
    qreal latRad = qgeocoordinate_degToRad(center.latitude());
    qreal lonRad = qgeocoordinate_degToRad(centerLon);
    qreal cosLatRad = std::cos(latRad);
    qreal sinLatRad = std::sin(latRad);
    qreal ratio = (distance / (qgeocoordinate_EARTH_MEAN_RADIUS * 1000.0));
    qreal cosRatio = std::cos(ratio);
    qreal sinRatio = std::sin(ratio);
    qreal sinLatRad_x_cosRatio = sinLatRad * cosRatio;
    qreal cosLatRad_x_sinRatio = cosLatRad * sinRatio;
    int idx = 0;
    for (int i = 0; i < steps; ++i) {
        qreal azimuthRad = 2 * M_PI * i / steps;
        qreal resultLatRad = std::asin(sinLatRad_x_cosRatio
                                   + cosLatRad_x_sinRatio * std::cos(azimuthRad));
        qreal resultLonRad = lonRad + std::atan2(std::sin(azimuthRad) * cosLatRad_x_sinRatio,
                                       cosRatio - sinLatRad * std::sin(resultLatRad));
        qreal lat2 = qgeocoordinate_radToDeg(resultLatRad);
        qreal lon2 = qgeocoordinate_radToDeg(resultLonRad);
        if (lon2 < -180.0) {
            lon2 += 360.0;
        } else if (lon2 > 180.0) {
            lon2 -= 360.0;
        }
        path << QGeoCoordinate(lat2, lon2, center.altitude());
        // Consider only points in the left half of the circle for the left bound.
        if (azimuthRad > M_PI) {
            if (lon2 > centerLon) // if point and center are on different hemispheres
                lon2 -= 360;
            if (lon2 < minLon) {
                minLon = lon2;
                idx = i;
            }
        }
    }
    leftBound = path.at(idx);
}

QDeclarativeCircleMapItem::QDeclarativeCircleMapItem(QQuickItem *parent)
:   QDeclarativeGeoMapItemBase(parent), color_(Qt::transparent), radius_(0), dirtyMaterial_(true),
    updatingGeometry_(false)
{
    setFlag(ItemHasContents, true);
    QObject::connect(&border_, SIGNAL(colorChanged(QColor)),
                     this, SLOT(markSourceDirtyAndUpdate()));
    QObject::connect(&border_, SIGNAL(widthChanged(qreal)),
                     this, SLOT(markSourceDirtyAndUpdate()));

    // assume that circles are not self-intersecting
    // to speed up processing
    // FIXME: unfortunately they self-intersect at the poles due to current drawing method
    // so the line is commented out until fixed
    //geometry_.setAssumeSimple(true);

}

QDeclarativeCircleMapItem::~QDeclarativeCircleMapItem()
{
}

/*!
    \qmlpropertygroup Location::MapCircle::border
    \qmlproperty int MapCircle::border.width
    \qmlproperty color MapCircle::border.color

    This property is part of the border group property.
    The border property holds the width and color used to draw the border of the circle.
    The width is in pixels and is independent of the zoom level of the map.

    The default values correspond to a black border with a width of 1 pixel.
    For no line, use a width of 0 or a transparent color.
*/
QDeclarativeMapLineProperties *QDeclarativeCircleMapItem::border()
{
    return &border_;
}

void QDeclarativeCircleMapItem::markSourceDirtyAndUpdate()
{
    geometry_.markSourceDirty();
    borderGeometry_.markSourceDirty();
    polishAndUpdate();
}

void QDeclarativeCircleMapItem::setMap(QDeclarativeGeoMap *quickMap, QGeoMap *map)
{
    QDeclarativeGeoMapItemBase::setMap(quickMap,map);
    if (map)
        markSourceDirtyAndUpdate();
}

/*!
    \qmlproperty coordinate MapCircle::center

    This property holds the central point about which the circle is defined.

    \sa radius
*/
void QDeclarativeCircleMapItem::setCenter(const QGeoCoordinate &center)
{
    if (center_ == center)
        return;

    center_ = center;
    markSourceDirtyAndUpdate();
    emit centerChanged(center_);
}

QGeoCoordinate QDeclarativeCircleMapItem::center()
{
    return center_;
}

/*!
    \qmlproperty color MapCircle::color

    This property holds the fill color of the circle when drawn. For no fill,
    use a transparent color.
*/
void QDeclarativeCircleMapItem::setColor(const QColor &color)
{
    if (color_ == color)
        return;
    color_ = color;
    dirtyMaterial_ = true;
    update();
    emit colorChanged(color_);
}

QColor QDeclarativeCircleMapItem::color() const
{
    return color_;
}

/*!
    \qmlproperty real MapCircle::radius

    This property holds the radius of the circle, in meters on the ground.

    \sa center
*/
void QDeclarativeCircleMapItem::setRadius(qreal radius)
{
    if (radius_ == radius)
        return;

    radius_ = radius;
    markSourceDirtyAndUpdate();
    emit radiusChanged(radius);
}

qreal QDeclarativeCircleMapItem::radius() const
{
    return radius_;
}

/*!
  \qmlproperty real MapCircle::opacity

  This property holds the opacity of the item.  Opacity is specified as a
  number between 0 (fully transparent) and 1 (fully opaque).  The default is 1.

  An item with 0 opacity will still receive mouse events. To stop mouse events, set the
  visible property of the item to false.
*/

/*!
    \internal
*/
QSGNode *QDeclarativeCircleMapItem::updateMapItemPaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);

    MapPolygonNode *node = static_cast<MapPolygonNode *>(oldNode);

    if (!node)
        node = new MapPolygonNode();

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
void QDeclarativeCircleMapItem::updatePolish()
{
    if (!map() || !center().isValid() || qIsNaN(radius_) || radius_ <= 0.0)
        return;

    QScopedValueRollback<bool> rollback(updatingGeometry_);
    updatingGeometry_ = true;

    if (geometry_.isSourceDirty()) {
        circlePath_.clear();
        calculatePeripheralPoints(circlePath_, center_, radius_, CircleSamples, geoLeftBound_);
    }

    int pathCount = circlePath_.size();
    bool preserve = preserveCircleGeometry(circlePath_, center_, radius_);
    geometry_.setPreserveGeometry(preserve, geoLeftBound_);
    geometry_.updateSourcePoints(*map(), circlePath_);
    if (crossEarthPole(center_, radius_) && circlePath_.size() == pathCount)
        geometry_.updateScreenPointsInvert(*map()); // invert fill area for really huge circles
    else geometry_.updateScreenPoints(*map());

    if (border_.color() != Qt::transparent && border_.width() > 0) {
        QList<QGeoCoordinate> closedPath = circlePath_;
        closedPath << closedPath.first();
        borderGeometry_.setPreserveGeometry(preserve, geoLeftBound_);
        borderGeometry_.updateSourcePoints(*map(), closedPath, geoLeftBound_);
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

    setPositionOnMap(circlePath_.at(0), geometry_.firstPointOffset());
}

/*!
    \internal
*/
void QDeclarativeCircleMapItem::afterViewportChanged(const QGeoMapViewportChangeEvent &event)
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

    if (event.centerChanged && crossEarthPole(center_, radius_)) {
        geometry_.markSourceDirty();
        borderGeometry_.markSourceDirty();
    }

    geometry_.markScreenDirty();
    borderGeometry_.markScreenDirty();
    polishAndUpdate();
}

/*!
    \internal
*/
bool QDeclarativeCircleMapItem::contains(const QPointF &point) const
{
    return (geometry_.contains(point) || borderGeometry_.contains(point));
}

/*!
    \internal
*/
void QDeclarativeCircleMapItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (updatingGeometry_ || newGeometry == oldGeometry) {
        QDeclarativeGeoMapItemBase::geometryChanged(newGeometry, oldGeometry);
        return;
    }

    QDoubleVector2D newPoint = QDoubleVector2D(x(),y()) + QDoubleVector2D(width(), height()) / 2;
    QGeoCoordinate newCoordinate = map()->itemPositionToCoordinate(newPoint, false);
    if (newCoordinate.isValid())
        setCenter(newCoordinate);

    // Not calling QDeclarativeGeoMapItemBase::geometryChanged() as it will be called from a nested
    // call to this function.
}

bool QDeclarativeCircleMapItem::preserveCircleGeometry (QList<QGeoCoordinate> &path,
                                    const QGeoCoordinate &center, qreal distance)
{
    // if circle crosses north/south pole, then don't preserve circular shape,
    if ( crossEarthPole(center, distance)) {
        updateCirclePathForRendering(path, center, distance);
        return false;
    }
    return true;

}


// A workaround for circle path to be drawn correctly using a polygon geometry
void QDeclarativeCircleMapItem::updateCirclePathForRendering(QList<QGeoCoordinate> &path,
                                                             const QGeoCoordinate &center,
                                                             qreal distance)
{
    qreal poleLat = 90;
    qreal distanceToNorthPole = center.distanceTo(QGeoCoordinate(poleLat, 0));
    qreal distanceToSouthPole = center.distanceTo(QGeoCoordinate(-poleLat, 0));
    bool crossNorthPole = distanceToNorthPole < distance;
    bool crossSouthPole = distanceToSouthPole < distance;
    if (!crossNorthPole && !crossSouthPole)
        return;
    QList<int> wrapPathIndex;
    // calculate actual width of map on screen in pixels
    QDoubleVector2D midPoint = map()->coordinateToItemPosition(map()->cameraData().center(), false);
    QDoubleVector2D midPointPlusOne(midPoint.x() + 1.0, midPoint.y());
    QGeoCoordinate coord1 = map()->itemPositionToCoordinate(midPointPlusOne, false);
    qreal geoDistance = coord1.longitude() - map()->cameraData().center().longitude();
    if ( geoDistance < 0 )
        geoDistance += 360;
    qreal mapWidth = 360.0 / geoDistance;
    mapWidth = qMin(static_cast<int>(mapWidth), map()->viewportWidth());
    QDoubleVector2D prev = map()->coordinateToItemPosition(path.at(0), false);
    // find the points in path where wrapping occurs
    for (int i = 1; i <= path.count(); ++i) {
        int index = i % path.count();
        QDoubleVector2D point = map()->coordinateToItemPosition(path.at(index), false);
        if ( (qAbs(point.x() - prev.x())) >= mapWidth/2.0 ) {
            wrapPathIndex << index;
            if (wrapPathIndex.size() == 2 || !(crossNorthPole && crossSouthPole))
                break;
        }
        prev = point;
    }
    // insert two additional coords at top/bottom map corners of the map for shape
    // to be drawn correctly
    if (wrapPathIndex.size() > 0) {
        qreal newPoleLat = 90;
        QGeoCoordinate wrapCoord = path.at(wrapPathIndex[0]);
        if (wrapPathIndex.size() == 2) {
            QGeoCoordinate wrapCoord2 = path.at(wrapPathIndex[1]);
            if (wrapCoord2.latitude() > wrapCoord.latitude())
                newPoleLat = -90;
        } else if (center.latitude() < 0) {
            newPoleLat = -90;
        }
        for (int i = 0; i < wrapPathIndex.size(); ++i) {
            int index = wrapPathIndex[i] == 0 ? 0 : wrapPathIndex[i] + i*2;
            int prevIndex = (index - 1) < 0 ? (path.count() - 1): index - 1;
            QGeoCoordinate coord0 = path.at(prevIndex);
            QGeoCoordinate coord1 = path.at(index);
            coord0.setLatitude(newPoleLat);
            coord1.setLatitude(newPoleLat);
            path.insert(index ,coord1);
            path.insert(index, coord0);
            newPoleLat = -newPoleLat;
        }
    }
}

//////////////////////////////////////////////////////////////////////

QT_END_NAMESPACE
