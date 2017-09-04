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

#include "qdeclarativepolylinemapitem_p.h"
#include "qlocationutils_p.h"
#include "error_messages_p.h"
#include "locationvaluetypehelper_p.h"
#include "qdoublevector2d_p.h"
#include <QtLocation/private/qgeomap_p.h>

#include <QtCore/QScopedValueRollback>
#include <QtQml/QQmlInfo>
#include <QtQml/private/qqmlengine_p.h>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <qnumeric.h>

#include <QtGui/private/qvectorpath_p.h>
#include <QtGui/private/qtriangulatingstroker_p.h>
#include <QtGui/private/qtriangulator_p.h>

#include <QtPositioning/private/qclipperutils_p.h>

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
    appearance. Setting the \l {Item::opacity}{opacity} property will force the object to
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

QList<QList<QDoubleVector2D> > QGeoMapPolylineGeometry::clipPath(const QGeoMap &map,
                                                           const QList<QDoubleVector2D> &path,
                                                           QDoubleVector2D &leftBoundWrapped)
{
    /*
     * Approach:
     * 1) project coordinates to wrapped web mercator, and do unwrapBelowX
     * 2) if the scene is tilted, clip the geometry against the visible region (this may generate multiple polygons)
     * 2.1) recalculate the origin and geoLeftBound to prevent these parameters from ending in unprojectable areas
     * 2.2) ensure the left bound does not wrap around due to QGeoCoordinate <-> clipper conversions
     */

    srcOrigin_ = geoLeftBound_;

    double unwrapBelowX = 0;
    leftBoundWrapped = map.geoProjection().wrapMapProjection(map.geoProjection().geoToMapProjection(geoLeftBound_));
    if (preserveGeometry_)
        unwrapBelowX = leftBoundWrapped.x();

    QList<QDoubleVector2D> wrappedPath;
    wrappedPath.reserve(path.size());
    QDoubleVector2D wrappedLeftBound(qInf(), qInf());
    // 1)
    for (int i = 0; i < path.size(); ++i) {
        const QDoubleVector2D &coord = path.at(i);
        QDoubleVector2D wrappedProjection = map.geoProjection().wrapMapProjection(coord);

        // We can get NaN if the map isn't set up correctly, or the projection
        // is faulty -- probably best thing to do is abort
        if (!qIsFinite(wrappedProjection.x()) || !qIsFinite(wrappedProjection.y()))
            return QList<QList<QDoubleVector2D> >();

        const bool isPointLessThanUnwrapBelowX = (wrappedProjection.x() < leftBoundWrapped.x());
        // unwrap x to preserve geometry if moved to border of map
        if (preserveGeometry_ && isPointLessThanUnwrapBelowX) {
            double distance = wrappedProjection.x() - unwrapBelowX;
            if (distance < 0.0)
                distance += 1.0;
            wrappedProjection.setX(unwrapBelowX + distance);
        }
        if (wrappedProjection.x() < wrappedLeftBound.x() || (wrappedProjection.x() == wrappedLeftBound.x() && wrappedProjection.y() < wrappedLeftBound.y())) {
            wrappedLeftBound = wrappedProjection;
        }
        wrappedPath.append(wrappedProjection);
    }

    // 2)
    QList<QList<QDoubleVector2D> > clippedPaths;
    const QList<QDoubleVector2D> &visibleRegion = map.geoProjection().visibleRegion();
    if (visibleRegion.size()) {
        c2t::clip2tri clipper;
        clipper.addSubjectPath(QClipperUtils::qListToPath(wrappedPath), false);
        clipper.addClipPolygon(QClipperUtils::qListToPath(visibleRegion));
        Paths res = clipper.execute(c2t::clip2tri::Intersection);
        clippedPaths = QClipperUtils::pathsToQList(res);

        // 2.1) update srcOrigin_ and leftBoundWrapped with the point with minimum X
        QDoubleVector2D lb(qInf(), qInf());
        for (const QList<QDoubleVector2D> &path: clippedPaths) {
            for (const QDoubleVector2D &p: path) {
                if (p == leftBoundWrapped) {
                    lb = p;
                    break;
                } else if (p.x() < lb.x() || (p.x() == lb.x() && p.y() < lb.y())) {
                    // y-minimization needed to find the same point on polygon and border
                    lb = p;
                }
            }
        }
        if (qIsInf(lb.x()))
            return QList<QList<QDoubleVector2D> >();

        // 2.2) Prevent the conversion to and from clipper from introducing negative offsets which
        //      in turn will make the geometry wrap around.
        lb.setX(qMax(wrappedLeftBound.x(), lb.x()));
        leftBoundWrapped = lb;
    } else {
        clippedPaths.append(wrappedPath);
    }

    return clippedPaths;
}

void QGeoMapPolylineGeometry::pathToScreen(const QGeoMap &map,
                                           const QList<QList<QDoubleVector2D> > &clippedPaths,
                                           const QDoubleVector2D &leftBoundWrapped)
{
    // 3) project the resulting geometry to screen position and calculate screen bounds
    double minX = qInf();
    double minY = qInf();
    double maxX = -qInf();
    double maxY = -qInf();

    srcOrigin_ = map.geoProjection().mapProjectionToGeo(map.geoProjection().unwrapMapProjection(leftBoundWrapped));
    QDoubleVector2D origin = map.geoProjection().wrappedMapProjectionToItemPosition(leftBoundWrapped);
    for (const QList<QDoubleVector2D> &path: clippedPaths) {
        QDoubleVector2D lastAddedPoint;
        for (int i = 0; i < path.size(); ++i) {
            QDoubleVector2D point = map.geoProjection().wrappedMapProjectionToItemPosition(path.at(i));

            point = point - origin; // (0,0) if point == geoLeftBound_

            minX = qMin(point.x(), minX);
            minY = qMin(point.y(), minY);
            maxX = qMax(point.x(), maxX);
            maxY = qMax(point.y(), maxY);

            if (i == 0) {
                srcPoints_ << point.x() << point.y();
                srcPointTypes_ << QPainterPath::MoveToElement;
                lastAddedPoint = point;
            } else {
                if ((point - lastAddedPoint).manhattanLength() > 3 ||
                        i == path.size() - 1) {
                    srcPoints_ << point.x() << point.y();
                    srcPointTypes_ << QPainterPath::LineToElement;
                    lastAddedPoint = point;
                }
            }
        }
    }

    sourceBounds_ = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

/*!
    \internal
*/
void QGeoMapPolylineGeometry::updateSourcePoints(const QGeoMap &map,
                                                 const QList<QDoubleVector2D> &path,
                                                 const QGeoCoordinate geoLeftBound)
{
    if (!sourceDirty_)
        return;

    geoLeftBound_ = geoLeftBound;

    // clear the old data and reserve enough memory
    srcPoints_.clear();
    srcPoints_.reserve(path.size() * 2);
    srcPointTypes_.clear();
    srcPointTypes_.reserve(path.size());

    /*
     * Approach:
     * 1) project coordinates to wrapped web mercator, and do unwrapBelowX
     * 2) if the scene is tilted, clip the geometry against the visible region (this may generate multiple polygons)
     * 3) project the resulting geometry to screen position and calculate screen bounds
     */

    QDoubleVector2D leftBoundWrapped;
    // 1, 2)
    const QList<QList<QDoubleVector2D> > &clippedPaths = clipPath(map, path, leftBoundWrapped);

    // 3)
    pathToScreen(map, clippedPaths, leftBoundWrapped);
}

////////////////////////////////////////////////////////////////////////////
#if 0 // Old polyline to viewport clipping code. Retaining it for now.
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
#endif
/*!
    \internal
*/
void QGeoMapPolylineGeometry::updateScreenPoints(const QGeoMap &map,
                                                 qreal strokeWidth)
{
    if (!screenDirty_)
        return;

    QPointF origin = map.geoProjection().coordinateToItemPosition(srcOrigin_, false).toPointF();

    if (!qIsFinite(origin.x()) || !qIsFinite(origin.y()) || srcPointTypes_.size() < 2) { // the line might have been clipped away.
        clear();
        return;
    }

    // Create the viewport rect in the same coordinate system
    // as the actual points
    QRectF viewport(0, 0, map.viewportWidth(), map.viewportHeight());
    viewport.adjust(-strokeWidth, -strokeWidth, strokeWidth, strokeWidth);
    viewport.translate(-1 * origin);

    // The geometry has already been clipped against the visible region projection in wrapped mercator space.
    QVector<qreal> points = srcPoints_;
    QVector<QPainterPath::ElementType> types = srcPointTypes_;

    QVectorPath vp(points.data(), types.size(), types.data());
    QTriangulatingStroker ts;
    // viewport is not used in the call below.
    ts.process(vp, QPen(QBrush(Qt::black), strokeWidth), viewport, QPainter::Qt4CompatiblePainting);

    clear();

    // Nothing is on the screen
    if (ts.vertexCount() == 0)
        return;

    // QTriangulatingStroker#vertexCount is actually the length of the array,
    // not the number of vertices
    screenVertices_.reserve(ts.vertexCount());

    QRectF bb;

    QPointF pt;
    const float *vs = ts.vertices();
    for (int i = 0; i < (ts.vertexCount()/2*2); i += 2) {
        pt = QPointF(vs[i], vs[i + 1]);
        screenVertices_ << pt;

        if (!qIsFinite(pt.x()) || !qIsFinite(pt.y()))
            break;

        if (!bb.contains(pt)) {
            if (pt.x() < bb.left())
                bb.setLeft(pt.x());

            if (pt.x() > bb.right())
                bb.setRight(pt.x());

            if (pt.y() < bb.top())
                bb.setTop(pt.y());

            if (pt.y() > bb.bottom())
                bb.setBottom(pt.y());
        }
    }

    screenBounds_ = bb;
    this->translate( -1 * sourceBounds_.topLeft());
}

QDeclarativePolylineMapItem::QDeclarativePolylineMapItem(QQuickItem *parent)
:   QDeclarativeGeoMapItemBase(parent), line_(this), dirtyMaterial_(true), updatingGeometry_(false)
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
    polishAndUpdate();
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::setMap(QDeclarativeGeoMap *quickMap, QGeoMap *map)
{
    QDeclarativeGeoMapItemBase::setMap(quickMap,map);
    if (map) {
        regenerateCache();
        geometry_.markSourceDirty();
        polishAndUpdate();
    }
}

/*!
    \qmlproperty list<coordinate> MapPolyline::path

    This property holds the ordered list of coordinates which
    define the polyline.
*/

QJSValue QDeclarativePolylineMapItem::path() const
{
    QQmlContext *context = QQmlEngine::contextForObject(this);
    QQmlEngine *engine = context->engine();
    QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(engine);

    QV4::Scope scope(v4);
    QV4::Scoped<QV4::ArrayObject> pathArray(scope, v4->newArrayObject(geopath_.path().length()));
    for (int i = 0; i < geopath_.path().length(); ++i) {
        const QGeoCoordinate &c = geopath_.coordinateAt(i);

        QV4::ScopedValue cv(scope, v4->fromVariant(QVariant::fromValue(c)));
        pathArray->putIndexed(i, cv);
    }

    return QJSValue(v4, pathArray.asReturnedValue());
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
            qmlWarning(this) << "Unsupported path type";
            return;
        }

        pathList.append(c);
    }

    setPathFromGeoList(pathList);
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::setPathFromGeoList(const QList<QGeoCoordinate> &path)
{
    if (geopath_.path() == path)
        return;

    geopath_.setPath(path);

    regenerateCache();
    geometry_.setPreserveGeometry(true, geopath_.boundingGeoRectangle().topLeft());
    markSourceDirtyAndUpdate();
    emit pathChanged();
}

/*!
    \qmlmethod int MapPolyline::pathLength()

    Returns the number of coordinates of the polyline.

    \since Qt Location 5.6

    \sa path
*/
int QDeclarativePolylineMapItem::pathLength() const
{
    return geopath_.path().length();
}

/*!
    \qmlmethod void MapPolyline::addCoordinate(coordinate)

    Adds a coordinate to the end of the path.

    \sa insertCoordinate, removeCoordinate, path
*/
void QDeclarativePolylineMapItem::addCoordinate(const QGeoCoordinate &coordinate)
{
    if (!coordinate.isValid())
        return;

    geopath_.addCoordinate(coordinate);

    updateCache();
    geometry_.setPreserveGeometry(true, geopath_.boundingGeoRectangle().topLeft());
    markSourceDirtyAndUpdate();
    emit pathChanged();
}

/*!
    \qmlmethod void MapPolyline::insertCoordinate(index, coordinate)

    Inserts a \a coordinate to the path at the given \a index.

    \since Qt Location 5.6

    \sa addCoordinate, removeCoordinate, path
*/
void QDeclarativePolylineMapItem::insertCoordinate(int index, const QGeoCoordinate &coordinate)
{
    if (index < 0 || index > geopath_.path().length())
        return;

    geopath_.insertCoordinate(index, coordinate);

    regenerateCache();
    geometry_.setPreserveGeometry(true, geopath_.boundingGeoRectangle().topLeft());
    markSourceDirtyAndUpdate();
    emit pathChanged();
}

/*!
    \qmlmethod void MapPolyline::replaceCoordinate(index, coordinate)

    Replaces the coordinate in the current path at the given \a index
    with the new \a coordinate.

    \since Qt Location 5.6

    \sa addCoordinate, insertCoordinate, removeCoordinate, path
*/
void QDeclarativePolylineMapItem::replaceCoordinate(int index, const QGeoCoordinate &coordinate)
{
    if (index < 0 || index >= geopath_.path().length())
        return;

    geopath_.replaceCoordinate(index, coordinate);

    regenerateCache();
    geometry_.setPreserveGeometry(true, geopath_.boundingGeoRectangle().topLeft());
    markSourceDirtyAndUpdate();
    emit pathChanged();
}

/*!
    \qmlmethod coordinate MapPolyline::coordinateAt(index)

    Gets the coordinate of the polyline at the given \a index.
    If the index is outside the path's bounds then an invalid
    coordinate is returned.

    \since Qt Location 5.6
*/
QGeoCoordinate QDeclarativePolylineMapItem::coordinateAt(int index) const
{
    if (index < 0 || index >= geopath_.path().length())
        return QGeoCoordinate();

    return geopath_.coordinateAt(index);
}

/*!
    \qmlmethod coordinate MapPolyline::containsCoordinate(coordinate)

    Returns true if the given \a coordinate is part of the path.

    \since Qt Location 5.6
*/
bool QDeclarativePolylineMapItem::containsCoordinate(const QGeoCoordinate &coordinate)
{
    return geopath_.containsCoordinate(coordinate);
}

/*!
    \qmlmethod void MapPolyline::removeCoordinate(coordinate)

    Removes \a coordinate from the path. If there are multiple instances of the
    same coordinate, the one added last is removed.

    If \a coordinate is not in the path this method does nothing.

    \sa addCoordinate, insertCoordinate, path
*/
void QDeclarativePolylineMapItem::removeCoordinate(const QGeoCoordinate &coordinate)
{
    int length = geopath_.path().length();
    geopath_.removeCoordinate(coordinate);
    if (geopath_.path().length() == length)
        return;

    regenerateCache();
    markSourceDirtyAndUpdate();
    emit pathChanged();
}

/*!
    \qmlmethod void MapPolyline::removeCoordinate(index)

    Removes a coordinate from the path at the given \a index.

    If \a index is invalid then this method does nothing.

    \since Qt Location 5.6

    \sa addCoordinate, insertCoordinate, path
*/
void QDeclarativePolylineMapItem::removeCoordinate(int index)
{
    if (index < 0 || index >= geopath_.path().length())
        return;

    geopath_.removeCoordinate(index);

    regenerateCache();
    geometry_.setPreserveGeometry(true, geopath_.boundingGeoRectangle().topLeft());
    markSourceDirtyAndUpdate();
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
    if (!map() || !geopath_.isValid() || updatingGeometry_ || newGeometry.topLeft() == oldGeometry.topLeft()) {
        QDeclarativeGeoMapItemBase::geometryChanged(newGeometry, oldGeometry);
        return;
    }
    // TODO: change the algorithm to preserve the distances and size!
    QGeoCoordinate newCenter = map()->geoProjection().itemPositionToCoordinate(QDoubleVector2D(newGeometry.center()), false);
    QGeoCoordinate oldCenter = map()->geoProjection().itemPositionToCoordinate(QDoubleVector2D(oldGeometry.center()), false);
    if (!newCenter.isValid() || !oldCenter.isValid())
        return;
    double offsetLongi = newCenter.longitude() - oldCenter.longitude();
    double offsetLati = newCenter.latitude() - oldCenter.latitude();
    if (offsetLati == 0.0 && offsetLongi == 0.0)
        return;

    geopath_.translate(offsetLati, offsetLongi);
    regenerateCache();
    geometry_.setPreserveGeometry(true, geopath_.boundingGeoRectangle().topLeft());
    markSourceDirtyAndUpdate();
    emit pathChanged();

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

    geometry_.setPreserveGeometry(true, geometry_.geoLeftBound());
    markSourceDirtyAndUpdate();
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::regenerateCache()
{
    if (!map())
        return;
    geopathProjected_.clear();
    geopathProjected_.reserve(geopath_.path().size());
    for (const QGeoCoordinate &c : geopath_.path())
        geopathProjected_ << map()->geoProjection().geoToMapProjection(c);
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::updateCache()
{
    if (!map())
        return;
    geopathProjected_ << map()->geoProjection().geoToMapProjection(geopath_.path().last());
}

/*!
    \internal
*/
void QDeclarativePolylineMapItem::updatePolish()
{
    if (!map() || geopath_.path().length() == 0)
        return;

    QScopedValueRollback<bool> rollback(updatingGeometry_);
    updatingGeometry_ = true;

    geometry_.updateSourcePoints(*map(), geopathProjected_, geopath_.boundingGeoRectangle().topLeft());
    geometry_.updateScreenPoints(*map(), line_.width());

    setWidth(geometry_.sourceBoundingBox().width());
    setHeight(geometry_.sourceBoundingBox().height());

    setPositionOnMap(geometry_.origin(), -1 * geometry_.sourceBoundingBox().topLeft());
}

void QDeclarativePolylineMapItem::markSourceDirtyAndUpdate()
{
    geometry_.markSourceDirty();
    polishAndUpdate();
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
    if (geometry_.isScreenDirty() || dirtyMaterial_ || !oldNode) {
        node->update(line_.color(), &geometry_);
        geometry_.setPreserveGeometry(false);
        geometry_.markClean();
        dirtyMaterial_ = false;
    }
    return node;
}

bool QDeclarativePolylineMapItem::contains(const QPointF &point) const
{
    QVector<QPointF> vertices = geometry_.vertices();
    QPolygonF tri;
    for (int i = 0; i < vertices.size(); ++i) {
        tri << vertices[i];
        if (tri.size() == 3) {
            if (tri.containsPoint(point,Qt::OddEvenFill))
                return true;
            tri.remove(0);
        }
    }

    return false;
}

const QGeoShape &QDeclarativePolylineMapItem::geoShape() const
{
    return geopath_;
}

QGeoMap::ItemType QDeclarativePolylineMapItem::itemType() const
{
    return QGeoMap::MapPolyline;
}

//////////////////////////////////////////////////////////////////////

/*!
    \internal
*/
MapPolylineNode::MapPolylineNode() :
    geometry_(QSGGeometry::defaultAttributes_Point2D(),0),
    blocked_(true)
{
    geometry_.setDrawingMode(QSGGeometry::DrawTriangleStrip);
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
