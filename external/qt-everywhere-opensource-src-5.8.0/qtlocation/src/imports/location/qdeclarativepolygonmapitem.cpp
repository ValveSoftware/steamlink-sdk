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

#include "qdeclarativepolygonmapitem_p.h"
#include "qgeocameracapabilities_p.h"
#include "qlocationutils_p.h"
#include "error_messages.h"
#include "locationvaluetypehelper_p.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/private/qtriangulator_p.h>
#include <QtQml/QQmlInfo>
#include <QtQml/private/qqmlengine_p.h>
#include <QPainter>
#include <QPainterPath>
#include <qnumeric.h>

#include "qdoublevector2d_p.h"

/* poly2tri triangulator includes */
#include "../../3rdparty/clip2tri/clip2tri.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapPolygon
    \instantiates QDeclarativePolygonMapItem
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.5

    \brief The MapPolygon type displays a polygon on a Map

    The MapPolygon type displays a polygon on a Map, specified in terms of an ordered list of
    \l {QtPositioning::coordinate}{coordinates}. For best appearance and results, polygons should be
    simple (not self-intersecting).

    The \l {QtPositioning::coordinate}{coordinates} on the path cannot be directly changed after
    being added to the Polygon.  Instead, copy the \l path into a var, modify the copy and reassign
    the copy back to the \l path.

    \code
    var path = mapPolygon.path;
    path[0].latitude = 5;
    mapPolygon.path = path;
    \endcode

    Coordinates can also be added and removed at any time using the \l addCoordinate and
    \l removeCoordinate methods.

    For drawing rectangles with "straight" edges (same latitude across one
    edge, same latitude across the other), the \l MapRectangle type provides
    a simpler, two-point API.

    By default, the polygon is displayed as a 1 pixel black border with no
    fill. To change its appearance, use the \l color, \l border.color and
    \l border.width properties.

    \note Since MapPolygons are geographic items, dragging a MapPolygon
    (through the use of \l MouseArea) causes its vertices to be
    recalculated in the geographic coordinate space. The edges retain the
    same geographic lengths (latitude and longitude differences between the
    vertices), but they remain straight. Apparent stretching of the item occurs
    when dragged to a different latitude.

    \section2 Performance

    MapPolygons have a rendering cost that is O(n) with respect to the number
    of vertices. This means that the per frame cost of having a Polygon on the
    Map grows in direct proportion to the number of points on the Polygon. There
    is an additional triangulation cost (approximately O(n log n)) which is
    currently paid with each frame, but in future may be paid only upon adding
    or removing points.

    Like the other map objects, MapPolygon is normally drawn without a smooth
    appearance. Setting the \l {Item::opacity}{opacity} property will force the object to
    be blended, which decreases performance considerably depending on the hardware in use.

    \section2 Example Usage

    The following snippet shows a MapPolygon being used to display a triangle,
    with three vertices near Brisbane, Australia. The triangle is filled in
    green, with a 1 pixel black border.

    \code
    Map {
        MapPolygon {
            color: 'green'
            path: [
                { latitude: -27, longitude: 153.0 },
                { latitude: -27, longitude: 154.1 },
                { latitude: -28, longitude: 153.5 }
            ]
        }
    }
    \endcode

    \image api-mappolygon.png
*/

struct Vertex
{
    QVector2D position;
};

QGeoMapPolygonGeometry::QGeoMapPolygonGeometry()
:   assumeSimple_(false)
{
}

/*!
    \internal
*/
void QGeoMapPolygonGeometry::updateSourcePoints(const QGeoMap &map,
                                                const QList<QGeoCoordinate> &path)
{
    if (!sourceDirty_)
        return;

    double minX = -1.0;

    // build the actual path
    QDoubleVector2D origin;
    QDoubleVector2D lastPoint;
    srcPath_ = QPainterPath();

    double unwrapBelowX = 0;
    if (preserveGeometry_ )
        unwrapBelowX = map.coordinateToItemPosition(geoLeftBound_, false).x();

    for (int i = 0; i < path.size(); ++i) {
        const QGeoCoordinate &coord = path.at(i);

        if (!coord.isValid())
            continue;

        QDoubleVector2D point = map.coordinateToItemPosition(coord, false);

        // We can get NaN if the map isn't set up correctly, or the projection
        // is faulty -- probably best thing to do is abort
        if (!qIsFinite(point.x()) || !qIsFinite(point.y()))
            return;

        // unwrap x to preserve geometry if moved to border of map
        if (preserveGeometry_ && point.x() < unwrapBelowX
                && !qFuzzyCompare(point.x(), unwrapBelowX)
                && !qFuzzyCompare(geoLeftBound_.longitude(), coord.longitude()))
            point.setX(unwrapBelowX + geoDistanceToScreenWidth(map, geoLeftBound_, coord));

        if (i == 0) {
            origin = point;
            minX = point.x();
            srcOrigin_ = coord;
            srcPath_.moveTo(point.toPointF() - origin.toPointF());
            lastPoint = point;
        } else {
            if (point.x() <= minX)
                minX = point.x();
            const QDoubleVector2D diff = (point - lastPoint);
            if (diff.x() * diff.x() + diff.y() * diff.y() >= 3.0) {
                srcPath_.lineTo(point.toPointF() - origin.toPointF());
                lastPoint = point;
            }
        }
    }

    srcPath_.closeSubpath();

    if (!assumeSimple_)
        srcPath_ = srcPath_.simplified();

    sourceBounds_ = srcPath_.boundingRect();
    geoLeftBound_ = map.itemPositionToCoordinate(QDoubleVector2D(minX, 0), false);
}

/*!
    \internal
*/
void QGeoMapPolygonGeometry::updateScreenPoints(const QGeoMap &map)
{
    if (!screenDirty_)
        return;

    if (map.viewportWidth() == 0 || map.viewportHeight() == 0) {
        clear();
        return;
    }

    QDoubleVector2D origin = map.coordinateToItemPosition(srcOrigin_, false);

    // Create the viewport rect in the same coordinate system
    // as the actual points
    QRectF viewport(0, 0, map.viewportWidth(), map.viewportHeight());
    viewport.translate(-1 * origin.toPointF());

    QPainterPath vpPath;
    vpPath.addRect(viewport);

    QPainterPath ppi;
    if (clipToViewport_)
        ppi = srcPath_.intersected(vpPath); // get the clipped version of the path
    else ppi = srcPath_;

    clear();

    // a polygon requires at least 3 points;
    if (ppi.elementCount() < 3)
        return;

    // Intersection between the viewport and a concave polygon can create multiple polygons
    // joined by a line at the viewport border, and poly2tri does not triangulate this very well
    // so use the full src path if the resulting polygon is concave.
    if (clipToViewport_) {
        int changeInX = 0;
        int changeInY = 0;
        QPainterPath::Element e1 = ppi.elementAt(1);
        QPainterPath::Element e = ppi.elementAt(0);
        QVector2D edgeA(e1.x - e.x ,e1.y - e.y);
        for (int i = 2; i <= ppi.elementCount(); ++i) {
            e = ppi.elementAt(i % ppi.elementCount());
            if (e.x == e1.x && e.y == e1.y)
                continue;
            QVector2D edgeB(e.x - e1.x, e.y - e1.y);
            if ((edgeA.x() < 0) == (edgeB.x() >= 0))
                changeInX++;
            if ((edgeA.y() < 0) == (edgeB.y() >= 0))
                changeInY++;
            edgeA = edgeB;
            e1 = e;
        }
        if (changeInX > 2 || changeInY > 2) // polygon is concave
            ppi = srcPath_;
    }

    // translate the path into top-left-centric coordinates
    QRectF bb = ppi.boundingRect();
    ppi.translate(-bb.left(), -bb.top());
    firstPointOffset_ = -1 * bb.topLeft();

    ppi.closeSubpath();

    screenOutline_ = ppi;

#if 1
    std::vector<std::vector<c2t::Point>> clipperPoints;
    clipperPoints.push_back(std::vector<c2t::Point>());
    std::vector<c2t::Point> &curPts = clipperPoints.front();
    curPts.reserve(ppi.elementCount());
    for (int i = 0; i < ppi.elementCount(); ++i) {
        const QPainterPath::Element e = ppi.elementAt(i);
        if (e.isMoveTo() || i == ppi.elementCount() - 1
                || (qAbs(e.x - curPts.front().x) < 0.1
                    && qAbs(e.y - curPts.front().y) < 0.1)) {
            if (curPts.size() > 2) {
                c2t::clip2tri *cdt = new c2t::clip2tri();
                std::vector<c2t::Point> outputTriangles;
                cdt->triangulate(clipperPoints, outputTriangles, std::vector<c2t::Point>());
                for (size_t i = 0; i < outputTriangles.size(); ++i) {
                    screenVertices_ << QPointF(outputTriangles[i].x, outputTriangles[i].y);
                }
                delete cdt;
            }
            curPts.clear();
            curPts.reserve(ppi.elementCount() - i);
            curPts.push_back( c2t::Point(e.x, e.y));
        } else if (e.isLineTo()) {
            curPts.push_back( c2t::Point(e.x, e.y));
        } else {
            qWarning("Unhandled element type in polygon painterpath");
        }
    }
#else // Old qTriangulate()-based code.
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
#endif

    screenBounds_ = ppi.boundingRect();
}

QDeclarativePolygonMapItem::QDeclarativePolygonMapItem(QQuickItem *parent)
:   QDeclarativeGeoMapItemBase(parent), color_(Qt::transparent), dirtyMaterial_(true),
    updatingGeometry_(false)
{
    setFlag(ItemHasContents, true);
    QObject::connect(&border_, SIGNAL(colorChanged(QColor)),
                     this, SLOT(handleBorderUpdated()));
    QObject::connect(&border_, SIGNAL(widthChanged(qreal)),
                     this, SLOT(handleBorderUpdated()));
}

/*!
    \internal
*/
void QDeclarativePolygonMapItem::handleBorderUpdated()
{
    borderGeometry_.markSourceDirty();
    polishAndUpdate();
}

QDeclarativePolygonMapItem::~QDeclarativePolygonMapItem()
{
}

/*!
    \qmlpropertygroup Location::MapPolygon::border
    \qmlproperty int MapPolygon::border.width
    \qmlproperty color MapPolygon::border.color

    This property is part of the border property group. The border property
    group holds the width and color used to draw the border of the polygon.

    The width is in pixels and is independent of the zoom level of the map.

    The default values correspond to a black border with a width of 1 pixel.
    For no line, use a width of 0 or a transparent color.
*/

QDeclarativeMapLineProperties *QDeclarativePolygonMapItem::border()
{
    return &border_;
}

/*!
    \internal
*/
void QDeclarativePolygonMapItem::setMap(QDeclarativeGeoMap *quickMap, QGeoMap *map)
{
    QDeclarativeGeoMapItemBase::setMap(quickMap,map);
    if (map) {
        geometry_.markSourceDirty();
        borderGeometry_.markSourceDirty();
        polishAndUpdate();
    }
}

/*!
    \qmlproperty list<coordinate> MapPolygon::path

    This property holds the ordered list of coordinates which
    define the polygon.

    \sa addCoordinate, removeCoordinate
*/
QJSValue QDeclarativePolygonMapItem::path() const
{
    QQmlContext *context = QQmlEngine::contextForObject(parent());
    QQmlEngine *engine = context->engine();
    QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(engine);

    QV4::Scope scope(v4);
    QV4::Scoped<QV4::ArrayObject> pathArray(scope, v4->newArrayObject(path_.length()));
    for (int i = 0; i < path_.length(); ++i) {
        const QGeoCoordinate &c = path_.at(i);

        QV4::ScopedValue cv(scope, v4->fromVariant(QVariant::fromValue(c)));
        pathArray->putIndexed(i, cv);
    }

    return QJSValue(v4, pathArray.asReturnedValue());
}

void QDeclarativePolygonMapItem::setPath(const QJSValue &value)
{
    if (!value.isArray())
        return;

    QList<QGeoCoordinate> pathList;
    quint32 length = value.property(QStringLiteral("length")).toUInt();
    for (quint32 i = 0; i < length; ++i) {
        bool ok;
        QGeoCoordinate c = parseCoordinate(value.property(i), &ok);

        if (!ok || !c.isValid()) {
            qmlInfo(this) << "Unsupported path type";
            return;
        }

        pathList.append(c);
    }

    if (path_ == pathList)
        return;

    path_ = pathList;
    geoLeftBound_ = QDeclarativePolylineMapItem::getLeftBound(path_, deltaXs_, minX_);
    geometry_.setPreserveGeometry(true, geoLeftBound_);
    borderGeometry_.setPreserveGeometry(true, geoLeftBound_);
    geometry_.markSourceDirty();
    borderGeometry_.markSourceDirty();
    polishAndUpdate();
    emit pathChanged();
}

/*!
    \qmlmethod void MapPolygon::addCoordinate(coordinate)

    Adds a coordinate to the path.

    \sa removeCoordinate, path
*/

void QDeclarativePolygonMapItem::addCoordinate(const QGeoCoordinate &coordinate)
{
    path_.append(coordinate);
    geoLeftBound_ = QDeclarativePolylineMapItem::getLeftBound(path_, deltaXs_, minX_, geoLeftBound_);
    geometry_.setPreserveGeometry(true, geoLeftBound_);
    borderGeometry_.setPreserveGeometry(true, geoLeftBound_);
    geometry_.markSourceDirty();
    borderGeometry_.markSourceDirty();
    polishAndUpdate();
    emit pathChanged();
}

/*!
    \qmlmethod void MapPolygon::removeCoordinate(coordinate)

    Removes \a coordinate from the path. If there are multiple instances of the
    same coordinate, the one added last is removed.

    If \a coordinate is not in the path this method does nothing.

    \sa addCoordinate, path
*/
void QDeclarativePolygonMapItem::removeCoordinate(const QGeoCoordinate &coordinate)
{
    int index = path_.lastIndexOf(coordinate);
    if (index == -1)
        return;

    path_.removeAt(index);
    geoLeftBound_ = QDeclarativePolylineMapItem::getLeftBound(path_, deltaXs_, minX_);
    geometry_.setPreserveGeometry(true, geoLeftBound_);
    borderGeometry_.setPreserveGeometry(true, geoLeftBound_);
    geometry_.markSourceDirty();
    borderGeometry_.markSourceDirty();
    polishAndUpdate();
    emit pathChanged();
}

/*!
    \qmlproperty color MapPolygon::color

    This property holds the color used to fill the polygon.

    The default value is transparent.
*/

QColor QDeclarativePolygonMapItem::color() const
{
    return color_;
}

void QDeclarativePolygonMapItem::setColor(const QColor &color)
{
    if (color_ == color)
        return;

    color_ = color;
    dirtyMaterial_ = true;
    update();
    emit colorChanged(color_);
}

/*!
    \internal
*/
QSGNode *QDeclarativePolygonMapItem::updateMapItemPaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
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
void QDeclarativePolygonMapItem::updatePolish()
{
    if (!map() || path_.count() == 0)
        return;

    QScopedValueRollback<bool> rollback(updatingGeometry_);
    updatingGeometry_ = true;

    geometry_.updateSourcePoints(*map(), path_);
    geometry_.updateScreenPoints(*map());

    QList<QGeoCoordinate> closedPath = path_;
    closedPath << closedPath.first();
    borderGeometry_.clear();
    borderGeometry_.updateSourcePoints(*map(), closedPath, geoLeftBound_);

    if (border_.color() != Qt::transparent && border_.width() > 0)
        borderGeometry_.updateScreenPoints(*map(), border_.width());

    QList<QGeoMapItemGeometry *> geoms;
    geoms << &geometry_ << &borderGeometry_;
    QRectF combined = QGeoMapItemGeometry::translateToCommonOrigin(geoms);

    setWidth(combined.width());
    setHeight(combined.height());

    setPositionOnMap(path_.at(0), -1 * geometry_.sourceBoundingBox().topLeft());
}

/*!
    \internal
*/
void QDeclarativePolygonMapItem::afterViewportChanged(const QGeoMapViewportChangeEvent &event)
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
    geometry_.setPreserveGeometry(true, geometry_.geoLeftBound());
    borderGeometry_.setPreserveGeometry(true, borderGeometry_.geoLeftBound());
    geometry_.markScreenDirty();
    borderGeometry_.markScreenDirty();
    polishAndUpdate();
}

/*!
    \internal
*/
bool QDeclarativePolygonMapItem::contains(const QPointF &point) const
{
    return (geometry_.contains(point) || borderGeometry_.contains(point));
}

/*!
    \internal
*/
void QDeclarativePolygonMapItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (updatingGeometry_ || newGeometry.topLeft() == oldGeometry.topLeft()) {
        QDeclarativeGeoMapItemBase::geometryChanged(newGeometry, oldGeometry);
        return;
    }

    QDoubleVector2D newPoint = QDoubleVector2D(x(),y()) + QDoubleVector2D(geometry_.firstPointOffset());
    QGeoCoordinate newCoordinate = map()->itemPositionToCoordinate(newPoint, false);
    if (newCoordinate.isValid()) {
        double firstLongitude = path_.at(0).longitude();
        double firstLatitude = path_.at(0).latitude();
        double minMaxLatitude = firstLatitude;
        // prevent dragging over valid min and max latitudes
        for (int i = 0; i < path_.count(); ++i) {
            double newLatitude = path_.at(i).latitude()
                    + newCoordinate.latitude() - firstLatitude;
            if (!QLocationUtils::isValidLat(newLatitude)) {
                if (qAbs(newLatitude) > qAbs(minMaxLatitude)) {
                    minMaxLatitude = newLatitude;
                }
            }
        }
        // calculate offset needed to re-position the item within map border
        double offsetLatitude = minMaxLatitude - QLocationUtils::clipLat(minMaxLatitude);
        for (int i = 0; i < path_.count(); ++i) {
            QGeoCoordinate coord = path_.at(i);
            // handle dateline crossing
            coord.setLongitude(QLocationUtils::wrapLong(coord.longitude()
                               + newCoordinate.longitude() - firstLongitude));
            coord.setLatitude(coord.latitude()
                              + newCoordinate.latitude() - firstLatitude - offsetLatitude);

            path_.replace(i, coord);
        }
        geoLeftBound_.setLongitude(QLocationUtils::wrapLong(geoLeftBound_.longitude()
                                                            + newCoordinate.longitude() - firstLongitude));
        geometry_.setPreserveGeometry(true, geoLeftBound_);
        borderGeometry_.setPreserveGeometry(true, geoLeftBound_);
        geometry_.markSourceDirty();
        borderGeometry_.markSourceDirty();
        polishAndUpdate();
        emit pathChanged();
    }

    // Not calling QDeclarativeGeoMapItemBase::geometryChanged() as it will be called from a nested
    // call to this function.
}

//////////////////////////////////////////////////////////////////////

MapPolygonNode::MapPolygonNode() :
    border_(new MapPolylineNode()),
    geometry_(QSGGeometry::defaultAttributes_Point2D(), 0),
    blocked_(true)
{
    geometry_.setDrawingMode(QSGGeometry::DrawTriangles);
    QSGGeometryNode::setMaterial(&fill_material_);
    QSGGeometryNode::setGeometry(&geometry_);

    appendChildNode(border_);
}

MapPolygonNode::~MapPolygonNode()
{
}

/*!
    \internal
*/
bool MapPolygonNode::isSubtreeBlocked() const
{
    return blocked_;
}

/*!
    \internal
*/
void MapPolygonNode::update(const QColor &fillColor, const QColor &borderColor,
                            const QGeoMapItemGeometry *fillShape,
                            const QGeoMapItemGeometry *borderShape)
{
    /* Do the border update first */
    border_->update(borderColor, borderShape);

    /* If we have neither fill nor border with valid points, block the whole
     * tree. We can't just block the fill without blocking the border too, so
     * we're a little conservative here (maybe at the expense of rendering
     * accuracy) */
    if (fillShape->size() == 0) {
        if (borderShape->size() == 0) {
            blocked_ = true;
            return;
        } else {
            blocked_ = false;
        }
    } else {
        blocked_ = false;
    }

    QSGGeometry *fill = QSGGeometryNode::geometry();
    fillShape->allocateAndFill(fill);
    markDirty(DirtyGeometry);

    if (fillColor != fill_material_.color()) {
        fill_material_.setColor(fillColor);
        setMaterial(&fill_material_);
        markDirty(DirtyMaterial);
    }
}
QT_END_NAMESPACE
