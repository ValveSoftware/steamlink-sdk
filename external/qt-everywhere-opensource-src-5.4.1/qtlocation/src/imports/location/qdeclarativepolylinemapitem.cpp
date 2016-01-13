/****************************************************************************
 **
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtLocation module of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:LGPL21$
 ** Commercial License Usage
 ** Licensees holding valid commercial Qt licenses may use this file in
 ** accordance with the commercial license agreement provided with the
 ** Software or, alternatively, in accordance with the terms contained in
 ** a written agreement between you and Digia. For licensing terms and
 ** conditions see http://qt.digia.com/licensing. For further information
 ** use the contact form at http://qt.digia.com/contact-us.
 **
 ** GNU Lesser General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU Lesser
 ** General Public License version 2.1 or version 3 as published by the Free
 ** Software Foundation and appearing in the file LICENSE.LGPLv21 and
 ** LICENSE.LGPLv3 included in the packaging of this file. Please review the
 ** following information to ensure the GNU Lesser General Public License
 ** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Digia gives you certain additional
 ** rights. These rights are described in the Digia Qt LGPL Exception
 ** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#include "qdeclarativepolylinemapitem_p.h"
#include "qgeocameracapabilities_p.h"
#include "qlocationutils_p.h"
#include "error_messages.h"
#include "locationvaluetypehelper_p.h"
#include "qdoublevector2d_p.h"

#include <QtCore/QScopedValueRollback>
#include <QtQml/QQmlInfo>
#include <QtQml/QQmlContext>
#include <QtQml/private/qqmlengine_p.h>
#include <private/qqmlvaluetypewrapper_p.h>
#include <private/qjsvalue_p.h>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <qnumeric.h>

#include <QtGui/private/qvectorpath_p.h>
#include <QtGui/private/qtriangulatingstroker_p.h>
#include <QtGui/private/qtriangulator_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapPolyline
    \instantiates QDeclarativePolylineMapItem
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.0

    \brief The MapPolyline type displays a polyline on a map.

    The MapPolyline type displays a polyline on a map, specified in terms of an ordered list of
    \l {coordinate}{coordinates}.  The \l {coordinate}{coordinates} on
    the path cannot be directly changed after being added to the Polyline.  Instead, copy the
    \l path into a var, modify the copy and reassign the copy back to the \l path.

    \code
    var path = mapPolyline.path;
    path[0].latitude = 5;
    mapPolyline.path = path;
    \endcode

    Coordinates can also be added and removed at any time using the \l addCoordinate and
    \l removeCoordinate methods.

    By default, the polyline is displayed as a 1-pixel thick black line. This
    can be changed using the \l line.width and \l line.color properties.

    \section2 Performance

    MapPolylines have a rendering cost that is O(n) with respect to the number
    of vertices. This means that the per frame cost of having a polyline on
    the Map grows in direct proportion to the number of points in the polyline.

    Like the other map objects, MapPolyline is normally drawn without a smooth
    appearance. Setting the \l {QtQuick::Item::opacity}{opacity} property will force the object to
    be blended, which decreases performance considerably depending on the hardware in use.

    \note MapPolylines are implemented using the OpenGL GL_LINES
    primitive. There have been occasional reports of issues and rendering
    inconsistencies on some (particularly quite old) platforms. No workaround
    is yet available for these issues.

    \section2 Example Usage

    The following snippet shows a MapPolyline with 4 points, making a shape
    like the top part of a "question mark" (?), near Brisbane, Australia.
    The line drawn is 3 pixels in width and green in color.

    \code
    Map {
        MapPolyline {
            line.width: 3
            line.color: 'green'
            path: [
                { latitude: -27, longitude: 153.0 },
                { latitude: -27, longitude: 154.1 },
                { latitude: -28, longitude: 153.5 },
                { latitude: -29, longitude: 153.5 }
            ]
        }
    }
    \endcode

    \image api-mappolyline.png
*/

QDeclarativeMapLineProperties::QDeclarativeMapLineProperties(QObject *parent) :
    QObject(parent),
    width_(1.0),
    color_(Qt::black)
{
}

/*!
    \internal
*/
QColor QDeclarativeMapLineProperties::color() const
{
    return color_;
}

/*!
    \internal
*/
void QDeclarativeMapLineProperties::setColor(const QColor &color)
{
    if (color_ == color)
        return;

    color_ = color;
    emit colorChanged(color_);
}

/*!
    \internal
*/
qreal QDeclarativeMapLineProperties::width() const
{
    return width_;
}

/*!
    \internal
*/
void QDeclarativeMapLineProperties::setWidth(qreal width)
{
    if (width_ == width)
        return;

    width_ = width;
    emit widthChanged(width_);
}

struct Vertex
{
    QVector2D position;
};

QGeoMapPolylineGeometry::QGeoMapPolylineGeometry()
{
}

/*!
    \internal
*/
void QGeoMapPolylineGeometry::updateSourcePoints(const QGeoMap &map,
                                                 const QList<QGeoCoordinate> &path)
{
    bool foundValid = false;
    double minX = -1.0;
    double minY = -1.0;
    double maxX = -1.0;
    double maxY = -1.0;

    if (!sourceDirty_)
        return;

    // clear the old data and reserve enough memory
    srcPoints_.clear();
    srcPoints_.reserve(path.size() * 2);
    srcPointTypes_.clear();
    srcPointTypes_.reserve(path.size());

    QDoubleVector2D origin, lastPoint, lastAddedPoint;

    double unwrapBelowX = 0;
    if (preserveGeometry_)
        unwrapBelowX = map.coordinateToScreenPosition(geoLeftBound_, false).x();

    for (int i = 0; i < path.size(); ++i) {
        const QGeoCoordinate &coord = path.at(i);

        if (!coord.isValid())
            continue;

        QDoubleVector2D point = map.coordinateToScreenPosition(coord, false);

        // We can get NaN if the map isn't set up correctly, or the projection
        // is faulty -- probably best thing to do is abort
        if (!qIsFinite(point.x()) || !qIsFinite(point.y()))
            return;

        // unwrap x to preserve geometry if moved to border of map
        if (preserveGeometry_ && point.x() < unwrapBelowX && !qFuzzyCompare(point.x(), unwrapBelowX))
            point.setX(unwrapBelowX + geoDistanceToScreenWidth(map, geoLeftBound_, coord));

        if (!foundValid) {
            foundValid = true;
            srcOrigin_ = coord;
            origin = point;
            point = QDoubleVector2D(0,0);

            minX = point.x();
            maxX = minX;
            minY = point.y();
            maxY = minY;

            srcPoints_ << point.x() << point.y();
            srcPointTypes_ << QPainterPath::MoveToElement;
            lastAddedPoint = point;
        } else {
            point -= origin;

            minX = qMin(point.x(), minX);
            minY = qMin(point.y(), minY);
            maxX = qMax(point.x(), maxX);
            maxY = qMax(point.y(), maxY);

            if ((point - lastAddedPoint).manhattanLength() > 3 ||
                    i == path.size() - 1) {
                srcPoints_ << point.x() << point.y();
                srcPointTypes_ << QPainterPath::LineToElement;
                lastAddedPoint = point;
            }
        }

        lastPoint = point;
    }

    sourceBounds_ = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    geoLeftBound_ = map.screenPositionToCoordinate(
                                    QDoubleVector2D(minX + origin.x(), minY + origin.y()), false);
}

////////////////////////////////////////////////////////////////////////////
/* Polyline clip */

enum ClipPointType {
    InsidePoint  = 0x00,
    LeftPoint    = 0x01,
    RightPoint   = 0x02,
    BottomPoint  = 0x04,
    TopPoint     = 0x08
};

static inline int clipPointType(qreal x, qreal y, const QRectF &rect)
{
    int type = InsidePoint;
    if (x < rect.left())
        type |= LeftPoint;
    else if (x > rect.right())
        type |= RightPoint;
    if (y < rect.top())
        type |= TopPoint;
    else if (y > rect.bottom())
        type |= BottomPoint;
    return type;
}

static void clipSegmentToRect(qreal x0, qreal y0, qreal x1, qreal y1,
                              const QRectF &clipRect,
                              QVector<qreal> &outPoints,
                              QVector<QPainterPath::ElementType> &outTypes)
{
    int type0 = clipPointType(x0, y0, clipRect);
    int type1 = clipPointType(x1, y1, clipRect);
    bool accept = false;

    while (true) {
        if (!(type0 | type1)) {
            accept = true;
            break;
        } else if (type0 & type1) {
            break;
        } else {
            qreal x = 0.0;
            qreal y = 0.0;
            int outsideType = type0 ? type0 : type1;

            if (outsideType & BottomPoint) {
                x = x0 + (x1 - x0) * (clipRect.bottom() - y0) / (y1 - y0);
                y = clipRect.bottom() - 0.1;
            } else if (outsideType & TopPoint) {
                x = x0 + (x1 - x0) * (clipRect.top() - y0) / (y1 - y0);
                y = clipRect.top() + 0.1;
            } else if (outsideType & RightPoint) {
                y = y0 + (y1 - y0) * (clipRect.right() - x0) / (x1 - x0);
                x = clipRect.right() - 0.1;
            } else if (outsideType & LeftPoint) {
                y = y0 + (y1 - y0) * (clipRect.left() - x0) / (x1 - x0);
                x = clipRect.left() + 0.1;
            }

            if (outsideType == type0) {
                x0 = x;
                y0 = y;
                type0 = clipPointType(x0, y0, clipRect);
            } else {
                x1 = x;
                y1 = y;
                type1 = clipPointType(x1, y1, clipRect);
            }
        }
    }

    if (accept) {
        if (outPoints.size() >= 2) {
            qreal lastX, lastY;
            lastY = outPoints.at(outPoints.size() - 1);
            lastX = outPoints.at(outPoints.size() - 2);

            if (!qFuzzyCompare(lastY, y0) || !qFuzzyCompare(lastX, x0)) {
                outTypes << QPainterPath::MoveToElement;
                outPoints << x0 << y0;
            }
        } else {
            outTypes << QPainterPath::MoveToElement;
            outPoints << x0 << y0;
        }

        outTypes << QPainterPath::LineToElement;
        outPoints << x1 << y1;
    }
}

static void clipPathToRect(const QVector<qreal> &points,
                           const QVector<QPainterPath::ElementType> &types,
                           const QRectF &clipRect,
                           QVector<qreal> &outPoints,
                           QVector<QPainterPath::ElementType> &outTypes)
{
    outPoints.clear();
    outPoints.reserve(points.size());
    outTypes.clear();
    outTypes.reserve(types.size());

    qreal lastX, lastY;
    for (int i = 0; i < types.size(); ++i) {
        if (i > 0 && types[i] != QPainterPath::MoveToElement) {
            qreal x = points[i * 2], y = points[i * 2 + 1];
            clipSegmentToRect(lastX, lastY, x, y, clipRect, outPoints, outTypes);
        }

        lastX = points[i * 2];
        lastY = points[i * 2 + 1];
    }
}

/*!
    \internal
*/
void QGeoMapPolylineGeometry::updateScreenPoints(const QGeoMap &map,
                                                 qreal strokeWidth)
{
    if (!screenDirty_)
        return;

    QPointF origin = map.coordinateToScreenPosition(srcOrigin_, false).toPointF();

    if (!qIsFinite(origin.x()) || !qIsFinite(origin.y())) {
        clear();
        return;
    }

    // Create the viewport rect in the same coordinate system
    // as the actual points
    QRectF viewport(0, 0, map.width(), map.height());
    viewport.adjust(-strokeWidth, -strokeWidth, strokeWidth, strokeWidth);
    viewport.translate(-1 * origin);

    // Perform clipping to the viewport limits
    QVector<qreal> points;
    QVector<QPainterPath::ElementType> types;

    if (clipToViewport_) {
        clipPathToRect(srcPoints_, srcPointTypes_, viewport, points, types);
    } else {
        points = srcPoints_;
        types = srcPointTypes_;
    }

    QVectorPath vp(points.data(), types.size(), types.data());
    QTriangulatingStroker ts;
    ts.process(vp, QPen(QBrush(Qt::black), strokeWidth), viewport, QPainter::Qt4CompatiblePainting);

    clear();

    // Nothing is on the screen
    if (ts.vertexCount() == 0)
        return;

    // QTriangulatingStroker#vertexCount is actually the length of the array,
    // not the number of vertices
    screenVertices_.reserve(ts.vertexCount());

    screenOutline_ = QPainterPath();

    QPolygonF tri;
    const float *vs = ts.vertices();
    for (int i = 0; i < (ts.vertexCount()/2*2); i += 2) {
        screenVertices_ << QPointF(vs[i], vs[i + 1]);

        if (!qIsFinite(vs[i]) || !qIsFinite(vs[i + 1]))
            break;

        tri << QPointF(vs[i], vs[i + 1]);
        if (tri.size() == 4) {
            tri.remove(0);
            screenOutline_.addPolygon(tri);
        }
    }

    QRectF bb = screenOutline_.boundingRect();
    screenBounds_ = bb;
    this->translate( -1 * sourceBounds_.topLeft());
}

QDeclarativePolylineMapItem::QDeclarativePolylineMapItem(QQuickItem *parent)
:   QDeclarativeGeoMapItemBase(parent), dirtyMaterial_(true), updatingGeometry_(false)
{
    setFlag(ItemHasContents, true);
    QObject::connect(&line_, SIGNAL(colorChanged(QColor)),
                     this, SLOT(updateAfterLinePropertiesChanged()));
    QObject::connect(&line_, SIGNAL(widthChanged(qreal)),
                     this, SLOT(updateAfterLinePropertiesChanged()));
}

QDeclarativePolylineMapItem::~QDeclarativePolylineMapItem()
{
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::updateAfterLinePropertiesChanged()
{
    // mark dirty just in case we're a width change
    geometry_.markSourceDirty();
    updateMapItem();
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::setMap(QDeclarativeGeoMap *quickMap, QGeoMap *map)
{
    QDeclarativeGeoMapItemBase::setMap(quickMap,map);
    if (map) {
        geometry_.markSourceDirty();
        updateMapItem();
    }
}

/*!
    \qmlproperty list<coordinate> MapPolyline::path

    This property holds the ordered list of coordinates which
    define the polyline.
*/

QJSValue QDeclarativePolylineMapItem::path() const
{
    QQmlContext *context = QQmlEngine::contextForObject(parent());
    QQmlEngine *engine = context->engine();
    QV8Engine *v8Engine = QQmlEnginePrivate::getV8Engine(engine);
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(v8Engine);

    QV4::Scope scope(v4);
    QV4::Scoped<QV4::ArrayObject> pathArray(scope, v4->newArrayObject(path_.length()));
    for (int i = 0; i < path_.length(); ++i) {
        const QGeoCoordinate &c = path_.at(i);

        QQmlValueType *vt = QQmlValueTypeFactory::valueType(qMetaTypeId<QGeoCoordinate>());
        QV4::ScopedValue cv(scope, QV4::QmlValueTypeWrapper::create(v8Engine, QVariant::fromValue(c), vt));

        pathArray->putIndexed(i, cv);
    }

    return new QJSValuePrivate(v4, QV4::ValueRef(pathArray));
}

void QDeclarativePolylineMapItem::setPath(const QJSValue &value)
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

    geometry_.markSourceDirty();
    updateMapItem();
    emit pathChanged();
}

/*!
    \qmlmethod MapPolyline::addCoordinate(coordinate)

    Adds a coordinate to the path.

    \sa removeCoordinate, path
*/

void QDeclarativePolylineMapItem::addCoordinate(const QGeoCoordinate &coordinate)
{
    path_.append(coordinate);

    geometry_.markSourceDirty();
    updateMapItem();
    emit pathChanged();
}

/*!
    \qmlmethod MapPolyline::removeCoordinate(coordinate)

    Removes a coordinate from the path. If there are multiple instances of the
    same coordinate, the one added last is removed.

    \sa addCoordinate, path
*/

void QDeclarativePolylineMapItem::removeCoordinate(const QGeoCoordinate &coordinate)
{
    int index = path_.lastIndexOf(coordinate);

    if (index == -1) {
        qmlInfo(this) << COORD_NOT_BELONG_TO << QStringLiteral("PolylineMapItem");
        return;
    }

    if (path_.count() < index + 1) {
        qmlInfo(this) << COORD_NOT_BELONG_TO << QStringLiteral("PolylineMapItem");
        return;
    }
    path_.removeAt(index);

    geometry_.markSourceDirty();
    updateMapItem();
    emit pathChanged();
}

/*!
    \qmlpropertygroup Location::MapPolyline::line
    \qmlproperty int MapPolyline::line.width
    \qmlproperty color MapPolyline::line.color

    This property is part of the line property group. The line
    property group holds the width and color used to draw the line.

    The width is in pixels and is independent of the zoom level of the map.
    The default values correspond to a black border with a width of 1 pixel.

    For no line, use a width of 0 or a transparent color.
*/

QDeclarativeMapLineProperties *QDeclarativePolylineMapItem::line()
{
    return &line_;
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (updatingGeometry_ || newGeometry.topLeft() == oldGeometry.topLeft()) {
        QDeclarativeGeoMapItemBase::geometryChanged(newGeometry, oldGeometry);
        return;
    }

    QDoubleVector2D newPoint = QDoubleVector2D(x(),y()) + QDoubleVector2D(geometry_.firstPointOffset());
    QGeoCoordinate newCoordinate = map()->screenPositionToCoordinate(newPoint, false);
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

        QGeoCoordinate leftBoundCoord = geometry_.geoLeftBound();
        leftBoundCoord.setLongitude(QLocationUtils::wrapLong(leftBoundCoord.longitude()
                           + newCoordinate.longitude() - firstLongitude));
        geometry_.setPreserveGeometry(true, leftBoundCoord);
        geometry_.markSourceDirty();
        updateMapItem();
        emit pathChanged();
    }

    // Not calling QDeclarativeGeoMapItemBase::geometryChanged() as it will be called from a nested
    // call to this function.
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::afterViewportChanged(const QGeoMapViewportChangeEvent &event)
{
    if (event.mapSize.width() <= 0 || event.mapSize.height() <= 0)
        return;

    // if the scene is tilted, we must regenerate our geometry every frame
    if (map()->cameraCapabilities().supportsTilting()
            && (event.cameraData.tilt() > 0.1
                || event.cameraData.tilt() < -0.1)) {
        geometry_.markSourceDirty();
    }

    // if the scene is rolled, we must regen too
    if (map()->cameraCapabilities().supportsRolling()
            && (event.cameraData.roll() > 0.1
                || event.cameraData.roll() < -0.1)) {
        geometry_.markSourceDirty();
    }

    // otherwise, only regen on rotate, resize and zoom
    if (event.bearingChanged || event.mapSizeChanged || event.zoomLevelChanged) {
        geometry_.markSourceDirty();
    }
    geometry_.setPreserveGeometry(true, geometry_.geoLeftBound());
    geometry_.markScreenDirty();
    updateMapItem();
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::updateMapItem()
{
    if (!map() || path_.count() == 0)
        return;

    QScopedValueRollback<bool> rollback(updatingGeometry_);
    updatingGeometry_ = true;

    geometry_.updateSourcePoints(*map(), path_);
    geometry_.updateScreenPoints(*map(), line_.width());

    setWidth(geometry_.sourceBoundingBox().width());
    setHeight(geometry_.sourceBoundingBox().height());

    setPositionOnMap(path_.at(0), -1 * geometry_.sourceBoundingBox().topLeft());
    update();
}

/*!
    \internal
*/
QSGNode *QDeclarativePolylineMapItem::updateMapItemPaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);

    MapPolylineNode *node = static_cast<MapPolylineNode *>(oldNode);

    if (!node) {
        node = new MapPolylineNode();
    }

    //TODO: update only material
    if (geometry_.isScreenDirty() || dirtyMaterial_) {
        node->update(line_.color(), &geometry_);
        geometry_.setPreserveGeometry(false);
        geometry_.markClean();
    }
    return node;
}

bool QDeclarativePolylineMapItem::contains(const QPointF &point) const
{
    return geometry_.contains(point);
}

//////////////////////////////////////////////////////////////////////

/*!
    \internal
*/
MapPolylineNode::MapPolylineNode() :
    geometry_(QSGGeometry::defaultAttributes_Point2D(),0),
    blocked_(true)
{
    geometry_.setDrawingMode(GL_TRIANGLE_STRIP);
    QSGGeometryNode::setMaterial(&fill_material_);
    QSGGeometryNode::setGeometry(&geometry_);
}


/*!
    \internal
*/
MapPolylineNode::~MapPolylineNode()
{
}

/*!
    \internal
*/
bool MapPolylineNode::isSubtreeBlocked() const
{
    return blocked_;
}

/*!
    \internal
*/
void MapPolylineNode::update(const QColor &fillColor,
                             const QGeoMapItemGeometry *shape)
{
    if (shape->size() == 0) {
        blocked_ = true;
        return;
    } else {
        blocked_ = false;
    }

    QSGGeometry *fill = QSGGeometryNode::geometry();
    shape->allocateAndFill(fill);
    markDirty(DirtyGeometry);

    if (fillColor != fill_material_.color()) {
        fill_material_.setColor(fillColor);
        setMaterial(&fill_material_);
        markDirty(DirtyMaterial);
    }
}

QT_END_NAMESPACE
