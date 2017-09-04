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

#include "qwebmercator_p.h"
#include <QtLocation/private/qgeomap_p.h>

#include <qmath.h>
#include <algorithm>

#include <QtCore/QScopedValueRollback>
#include <QPen>
#include <QPainter>
#include <QtGui/private/qtriangulator_p.h>

#include "qdoublevector2d_p.h"
#include "qlocationutils_p.h"
#include "qgeocircle.h"

/* poly2tri triangulator includes */
#include <common/shapes.h>
#include <sweep/cdt.h>

#include <QtPositioning/private/qclipperutils_p.h>

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
void QGeoMapCircleGeometry::updateScreenPointsInvert(const QList<QDoubleVector2D> &circlePath, const QGeoMap &map)
{
    // Not checking for !screenDirty anymore, as everything is now recalculated.
    clear();
    if (map.viewportWidth() == 0 || map.viewportHeight() == 0 || circlePath.size() < 3) // a circle requires at least 3 points;
        return;

    /*
     * No special case for no tilting as these items are very rare, and usually at most one per map.
     *
     * Approach:
     * 1) subtract the circle from a rectangle filling the whole map, *in wrapped mercator space*
     * 2) clip the resulting geometries against the visible region, *in wrapped mercator space*
     * 3) create a QPainterPath with each of the resulting polygons projected to screen
     * 4) use qTriangulate() to triangulate the painter path
     */

    // 1)
    double topLati = QLocationUtils::mercatorMaxLatitude();
    double bottomLati = -(QLocationUtils::mercatorMaxLatitude());
    double leftLongi = QLocationUtils::mapLeftLongitude(map.cameraData().center().longitude());
    double rightLongi = QLocationUtils::mapRightLongitude(map.cameraData().center().longitude());

    srcOrigin_ = QGeoCoordinate(topLati,leftLongi);
    QDoubleVector2D tl = map.geoProjection().geoToWrappedMapProjection(QGeoCoordinate(topLati,leftLongi));
    QDoubleVector2D tr = map.geoProjection().geoToWrappedMapProjection(QGeoCoordinate(topLati,rightLongi));
    QDoubleVector2D br = map.geoProjection().geoToWrappedMapProjection(QGeoCoordinate(bottomLati,rightLongi));
    QDoubleVector2D bl = map.geoProjection().geoToWrappedMapProjection(QGeoCoordinate(bottomLati,leftLongi));

    QList<QDoubleVector2D> fill;
    fill << tl << tr << br << bl;

    QList<QDoubleVector2D> hole;
    for (const QDoubleVector2D &c: circlePath)
        hole << map.geoProjection().wrapMapProjection(c);

    c2t::clip2tri clipper;
    clipper.addSubjectPath(QClipperUtils::qListToPath(fill), true);
    clipper.addClipPolygon(QClipperUtils::qListToPath(hole));
    Paths difference = clipper.execute(c2t::clip2tri::Difference, QtClipperLib::pftEvenOdd, QtClipperLib::pftEvenOdd);

    // 2)
    QDoubleVector2D lb = map.geoProjection().geoToWrappedMapProjection(srcOrigin_);
    QList<QList<QDoubleVector2D> > clippedPaths;
    const QList<QDoubleVector2D> &visibleRegion = map.geoProjection().visibleRegion();
    if (visibleRegion.size()) {
        clipper.clearClipper();
        for (const Path &p: difference)
            clipper.addSubjectPath(p, true);
        clipper.addClipPolygon(QClipperUtils::qListToPath(visibleRegion));
        Paths res = clipper.execute(c2t::clip2tri::Intersection, QtClipperLib::pftEvenOdd, QtClipperLib::pftEvenOdd);
        clippedPaths = QClipperUtils::pathsToQList(res);

        // 2.1) update srcOrigin_ with the point with minimum X/Y
        lb = QDoubleVector2D(qInf(), qInf());
        for (const QList<QDoubleVector2D> &path: clippedPaths) {
            for (const QDoubleVector2D &p: path) {
                if (p.x() < lb.x() || (p.x() == lb.x() && p.y() < lb.y())) {
                    lb = p;
                }
            }
        }
        if (qIsInf(lb.x()))
            return;

        // Prevent the conversion to and from clipper from introducing negative offsets which
        // in turn will make the geometry wrap around.
        lb.setX(qMax(tl.x(), lb.x()));
        srcOrigin_ = map.geoProjection().mapProjectionToGeo(map.geoProjection().unwrapMapProjection(lb));
    } else {
        clippedPaths = QClipperUtils::pathsToQList(difference);
    }

    //3)
    QDoubleVector2D origin = map.geoProjection().wrappedMapProjectionToItemPosition(lb);

    QPainterPath ppi;
    for (const QList<QDoubleVector2D> &path: clippedPaths) {
        QDoubleVector2D lastAddedPoint;
        for (int i = 0; i < path.size(); ++i) {
            QDoubleVector2D point = map.geoProjection().wrappedMapProjectionToItemPosition(path.at(i));
            //point = point - origin; // Do this using ppi.translate()

            if (i == 0) {
                ppi.moveTo(point.toPointF());
                lastAddedPoint = point;
            } else {
                if ((point - lastAddedPoint).manhattanLength() > 3 ||
                        i == path.size() - 1) {
                    ppi.lineTo(point.toPointF());
                    lastAddedPoint = point;
                }
            }
        }
        ppi.closeSubpath();
    }
    ppi.translate(-1 * origin.toPointF());

    QTriangleSet ts = qTriangulate(ppi);
    qreal *vx = ts.vertices.data();

    screenIndices_.reserve(ts.indices.size());
    screenVertices_.reserve(ts.vertices.size());

    if (ts.indices.type() == QVertexIndexVector::UnsignedInt) {
        const quint32 *ix = reinterpret_cast<const quint32 *>(ts.indices.data());
        for (int i = 0; i < (ts.indices.size()/3*3); ++i)
            screenIndices_ << ix[i];
    } else {
        const quint16 *ix = reinterpret_cast<const quint16 *>(ts.indices.data());
        for (int i = 0; i < (ts.indices.size()/3*3); ++i)
            screenIndices_ << ix[i];
    }
    for (int i = 0; i < (ts.vertices.size()/2*2); i += 2)
        screenVertices_ << QPointF(vx[i], vx[i + 1]);

    screenBounds_ = ppi.boundingRect();
    sourceBounds_ = screenBounds_;
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
                                      int steps)
{
    // Calculate points based on great-circle distance
    // Calculation is the same as GeoCoordinate's atDistanceAndAzimuth function
    // but tweaked here for computing multiple points

    // pre-calculations
    steps = qMax(steps, 3);
    qreal centerLon = center.longitude();
    qreal latRad = QLocationUtils::radians(center.latitude());
    qreal lonRad = QLocationUtils::radians(centerLon);
    qreal cosLatRad = std::cos(latRad);
    qreal sinLatRad = std::sin(latRad);
    qreal ratio = (distance / (QLocationUtils::earthMeanRadius()));
    qreal cosRatio = std::cos(ratio);
    qreal sinRatio = std::sin(ratio);
    qreal sinLatRad_x_cosRatio = sinLatRad * cosRatio;
    qreal cosLatRad_x_sinRatio = cosLatRad * sinRatio;
    for (int i = 0; i < steps; ++i) {
        qreal azimuthRad = 2 * M_PI * i / steps;
        qreal resultLatRad = std::asin(sinLatRad_x_cosRatio
                                   + cosLatRad_x_sinRatio * std::cos(azimuthRad));
        qreal resultLonRad = lonRad + std::atan2(std::sin(azimuthRad) * cosLatRad_x_sinRatio,
                                       cosRatio - sinLatRad * std::sin(resultLatRad));
        qreal lat2 = QLocationUtils::degrees(resultLatRad);
        qreal lon2 = QLocationUtils::wrapLong(QLocationUtils::degrees(resultLonRad));

        path << QGeoCoordinate(lat2, lon2, center.altitude());
    }
}

QDeclarativeCircleMapItem::QDeclarativeCircleMapItem(QQuickItem *parent)
:   QDeclarativeGeoMapItemBase(parent), border_(this), color_(Qt::transparent), dirtyMaterial_(true),
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
    if (!map)
        return;
    updateCirclePath();
    markSourceDirtyAndUpdate();
}

/*!
    \qmlproperty coordinate MapCircle::center

    This property holds the central point about which the circle is defined.

    \sa radius
*/
void QDeclarativeCircleMapItem::setCenter(const QGeoCoordinate &center)
{
    if (circle_.center() == center)
        return;

    circle_.setCenter(center);
    updateCirclePath();
    markSourceDirtyAndUpdate();
    emit centerChanged(center);
}

QGeoCoordinate QDeclarativeCircleMapItem::center()
{
    return circle_.center();
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
    if (circle_.radius() == radius)
        return;

    circle_.setRadius(radius);
    updateCirclePath();
    markSourceDirtyAndUpdate();
    emit radiusChanged(radius);
}

qreal QDeclarativeCircleMapItem::radius() const
{
    return circle_.radius();
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
    if (!map() || !circle_.isValid())
        return;

    QScopedValueRollback<bool> rollback(updatingGeometry_);
    updatingGeometry_ = true;

    QList<QDoubleVector2D> circlePath = circlePath_;

    int pathCount = circlePath.size();
    bool preserve = preserveCircleGeometry(circlePath, circle_.center(), circle_.radius());
    geometry_.setPreserveGeometry(true, circle_.boundingGeoRectangle().topLeft()); // to set the geoLeftBound_
    geometry_.setPreserveGeometry(preserve, circle_.boundingGeoRectangle().topLeft());

    bool invertedCircle = false;
    if (crossEarthPole(circle_.center(), circle_.radius()) && circlePath.size() == pathCount) {
        geometry_.updateScreenPointsInvert(circlePath, *map()); // invert fill area for really huge circles
        invertedCircle = true;
    } else {
        geometry_.updateSourcePoints(*map(), circlePath);
        geometry_.updateScreenPoints(*map());
    }

    borderGeometry_.clear();
    QList<QGeoMapItemGeometry *> geoms;
    geoms << &geometry_;

    if (border_.color() != Qt::transparent && border_.width() > 0) {
        QList<QDoubleVector2D> closedPath = circlePath;
        closedPath << closedPath.first();

        if (invertedCircle) {
            closedPath = circlePath_;
            closedPath << closedPath.first();
            std::reverse(closedPath.begin(), closedPath.end());
        }

        borderGeometry_.setPreserveGeometry(true, circle_.boundingGeoRectangle().topLeft()); // to set the geoLeftBound_
        borderGeometry_.setPreserveGeometry(preserve, circle_.boundingGeoRectangle().topLeft());

        // Use srcOrigin_ from fill geometry after clipping to ensure that translateToCommonOrigin won't fail.
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
void QDeclarativeCircleMapItem::afterViewportChanged(const QGeoMapViewportChangeEvent &event)
{
    if (event.mapSize.width() <= 0 || event.mapSize.height() <= 0)
        return;

    markSourceDirtyAndUpdate();
}

/*!
    \internal
*/
void QDeclarativeCircleMapItem::updateCirclePath()
{
    if (!map())
        return;
    QList<QGeoCoordinate> path;
    calculatePeripheralPoints(path, circle_.center(), circle_.radius(), CircleSamples);
    circlePath_.clear();
    for (const QGeoCoordinate &c : path)
        circlePath_ << map()->geoProjection().geoToMapProjection(c);
}

/*!
    \internal
*/
bool QDeclarativeCircleMapItem::contains(const QPointF &point) const
{
    return (geometry_.contains(point) || borderGeometry_.contains(point));
}

const QGeoShape &QDeclarativeCircleMapItem::geoShape() const
{
    return circle_;
}

QGeoMap::ItemType QDeclarativeCircleMapItem::itemType() const
{
    return QGeoMap::MapCircle;
}

/*!
    \internal
*/
void QDeclarativeCircleMapItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (!map() || !circle_.isValid() || updatingGeometry_ || newGeometry == oldGeometry) {
        QDeclarativeGeoMapItemBase::geometryChanged(newGeometry, oldGeometry);
        return;
    }

    QDoubleVector2D newPoint = QDoubleVector2D(x(),y()) + QDoubleVector2D(width(), height()) / 2;
    QGeoCoordinate newCoordinate = map()->geoProjection().itemPositionToCoordinate(newPoint, false);
    if (newCoordinate.isValid())
        setCenter(newCoordinate);

    // Not calling QDeclarativeGeoMapItemBase::geometryChanged() as it will be called from a nested
    // call to this function.
}

bool QDeclarativeCircleMapItem::preserveCircleGeometry (QList<QDoubleVector2D> &path,
                                    const QGeoCoordinate &center, qreal distance)
{
    // if circle crosses north/south pole, then don't preserve circular shape,
    if ( crossEarthPole(center, distance)) {
        updateCirclePathForRendering(path, center, distance);
        return false;
    }
    return true;

}

/*
 * A workaround for circle path to be drawn correctly using a polygon geometry
 * This method generates a polygon like
 *  _____________
 *  |           |
 *   \         /
 *    |       |
 *   /         \
 *  |           |
 *  -------------
 *
 * or a polygon like
 *
 *  ______________
 *  |    ____    |
 *   \__/    \__/
 */
void QDeclarativeCircleMapItem::updateCirclePathForRendering(QList<QDoubleVector2D> &path,
                                                             const QGeoCoordinate &center,
                                                             qreal distance)
{
    qreal poleLat = 90;
    qreal distanceToNorthPole = center.distanceTo(QGeoCoordinate(poleLat, 0));
    qreal distanceToSouthPole = center.distanceTo(QGeoCoordinate(-poleLat, 0));
    bool crossNorthPole = distanceToNorthPole < distance;
    bool crossSouthPole = distanceToSouthPole < distance;

    QList<int> wrapPathIndex;
    QDoubleVector2D prev = map()->geoProjection().wrapMapProjection(path.at(0));

    for (int i = 1; i <= path.count(); ++i) {
        int index = i % path.count();
        QDoubleVector2D point = map()->geoProjection().wrapMapProjection(path.at(index));
        double diff = qAbs(point.x() - prev.x());
        if (diff > 0.5) {
            continue;
        }
    }

    // find the points in path where wrapping occurs
    for (int i = 1; i <= path.count(); ++i) {
        int index = i % path.count();
        QDoubleVector2D point = map()->geoProjection().wrapMapProjection(path.at(index));
        if ( (qAbs(point.x() - prev.x())) >= 0.5 ) {
            wrapPathIndex << index;
            if (wrapPathIndex.size() == 2 || !(crossNorthPole && crossSouthPole))
                break;
        }
        prev = point;
    }
    // insert two additional coords at top/bottom map corners of the map for shape
    // to be drawn correctly
    if (wrapPathIndex.size() > 0) {
        qreal newPoleLat = 0; // 90 latitude
        QDoubleVector2D wrapCoord = path.at(wrapPathIndex[0]);
        if (wrapPathIndex.size() == 2) {
            QDoubleVector2D wrapCoord2 = path.at(wrapPathIndex[1]);
            if (wrapCoord2.y() < wrapCoord.y())
                newPoleLat = 1; // -90 latitude
        } else if (center.latitude() < 0) {
            newPoleLat = 1; // -90 latitude
        }
        for (int i = 0; i < wrapPathIndex.size(); ++i) {
            int index = wrapPathIndex[i] == 0 ? 0 : wrapPathIndex[i] + i*2;
            int prevIndex = (index - 1) < 0 ? (path.count() - 1): index - 1;
            QDoubleVector2D coord0 = path.at(prevIndex);
            QDoubleVector2D coord1 = path.at(index);
            coord0.setY(newPoleLat);
            coord1.setY(newPoleLat);
            path.insert(index ,coord1);
            path.insert(index, coord0);
            newPoleLat = 1.0 - newPoleLat;
        }
    }
}

//////////////////////////////////////////////////////////////////////

QT_END_NAMESPACE
