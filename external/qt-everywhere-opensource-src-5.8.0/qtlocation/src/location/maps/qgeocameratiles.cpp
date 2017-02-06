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
#include "qgeocameratiles_p.h"
#include "qgeocameradata_p.h"
#include "qgeotilespec_p.h"
#include "qgeomaptype_p.h"

#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QtPositioning/private/qdoublevector3d_p.h>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QSize>
#include <cmath>

QT_BEGIN_NAMESPACE

struct Frustum
{
    QDoubleVector3D topLeftNear;
    QDoubleVector3D topLeftFar;
    QDoubleVector3D topRightNear;
    QDoubleVector3D topRightFar;
    QDoubleVector3D bottomLeftNear;
    QDoubleVector3D bottomLeftFar;
    QDoubleVector3D bottomRightNear;
    QDoubleVector3D bottomRightFar;
};

typedef QVector<QDoubleVector3D> PolygonVector;

class QGeoCameraTilesPrivate
{
public:
    QGeoCameraTilesPrivate();
    ~QGeoCameraTilesPrivate();

    QString m_pluginString;
    QGeoMapType m_mapType;
    int m_mapVersion;
    QGeoCameraData m_camera;
    QSize m_screenSize;
    int m_tileSize;
    QSet<QGeoTileSpec> m_tiles;

    int m_intZoomLevel;
    int m_sideLength;

    bool m_dirtyGeometry;
    bool m_dirtyMetadata;

    double m_viewExpansion;
    void updateMetadata();
    void updateGeometry();

    Frustum createFrustum(double fieldOfViewGradient) const;

    class LengthSorter
    {
    public:
        QDoubleVector3D base;
        bool operator()(const QDoubleVector3D &lhs, const QDoubleVector3D &rhs)
        {
            return (lhs - base).lengthSquared() < (rhs - base).lengthSquared();
        }
    };

    void appendZIntersects(const QDoubleVector3D &start, const QDoubleVector3D &end, double z, QVector<QDoubleVector3D> &results) const;
    PolygonVector frustumFootprint(const Frustum &frustum) const;

    QPair<PolygonVector, PolygonVector> splitPolygonAtAxisValue(const PolygonVector &polygon, int axis, double value) const;
    QPair<PolygonVector, PolygonVector> clipFootprintToMap(const PolygonVector &footprint) const;

    QList<QPair<double, int> > tileIntersections(double p1, int t1, double p2, int t2) const;
    QSet<QGeoTileSpec> tilesFromPolygon(const PolygonVector &polygon) const;

    struct TileMap
    {
        TileMap();

        void add(int tileX, int tileY);

        QMap<int, QPair<int, int> > data;
    };
};

QGeoCameraTiles::QGeoCameraTiles()
    : d_ptr(new QGeoCameraTilesPrivate()) {}

QGeoCameraTiles::~QGeoCameraTiles()
{
}

void QGeoCameraTiles::setCameraData(const QGeoCameraData &camera)
{
    if (d_ptr->m_camera == camera)
        return;

    d_ptr->m_dirtyGeometry = true;
    d_ptr->m_camera = camera;
    d_ptr->m_intZoomLevel = static_cast<int>(std::floor(d_ptr->m_camera.zoomLevel()));
    d_ptr->m_sideLength = 1 << d_ptr->m_intZoomLevel;
}

QGeoCameraData QGeoCameraTiles::cameraData() const
{
    return d_ptr->m_camera;
}

void QGeoCameraTiles::setScreenSize(const QSize &size)
{
    if (d_ptr->m_screenSize == size)
        return;

    d_ptr->m_dirtyGeometry = true;
    d_ptr->m_screenSize = size;
}

void QGeoCameraTiles::setPluginString(const QString &pluginString)
{
    if (d_ptr->m_pluginString == pluginString)
        return;

    d_ptr->m_dirtyMetadata = true;
    d_ptr->m_pluginString = pluginString;
}

void QGeoCameraTiles::setMapType(const QGeoMapType &mapType)
{
    if (d_ptr->m_mapType == mapType)
        return;

    d_ptr->m_dirtyMetadata = true;
    d_ptr->m_mapType = mapType;
}

QGeoMapType QGeoCameraTiles::activeMapType() const
{
    return d_ptr->m_mapType;
}

void QGeoCameraTiles::setMapVersion(int mapVersion)
{
    if (d_ptr->m_mapVersion == mapVersion)
        return;

    d_ptr->m_dirtyMetadata = true;
    d_ptr->m_mapVersion = mapVersion;
}

void QGeoCameraTiles::setTileSize(int tileSize)
{
    if (d_ptr->m_tileSize == tileSize)
        return;

    d_ptr->m_dirtyGeometry = true;
    d_ptr->m_tileSize = tileSize;
}

void QGeoCameraTiles::setViewExpansion(double viewExpansion)
{
    d_ptr->m_viewExpansion = viewExpansion;
    d_ptr->m_dirtyGeometry = true;
}

int QGeoCameraTiles::tileSize() const
{
    return d_ptr->m_tileSize;
}

const QSet<QGeoTileSpec>& QGeoCameraTiles::createTiles()
{
    if (d_ptr->m_dirtyGeometry) {
        d_ptr->m_tiles.clear();
        d_ptr->updateGeometry();
        d_ptr->m_dirtyGeometry = false;
    }

    if (d_ptr->m_dirtyMetadata) {
        d_ptr->updateMetadata();
        d_ptr->m_dirtyMetadata = false;
    }

    return d_ptr->m_tiles;
}

QGeoCameraTilesPrivate::QGeoCameraTilesPrivate()
:   m_mapVersion(-1),
    m_tileSize(0),
    m_intZoomLevel(0),
    m_sideLength(0),
    m_dirtyGeometry(false),
    m_dirtyMetadata(false),
    m_viewExpansion(1.0)
{
}

QGeoCameraTilesPrivate::~QGeoCameraTilesPrivate() {}

void QGeoCameraTilesPrivate::updateMetadata()
{
    typedef QSet<QGeoTileSpec>::const_iterator iter;

    QSet<QGeoTileSpec> newTiles;

    iter i = m_tiles.constBegin();
    iter end = m_tiles.constEnd();

    for (; i != end; ++i) {
        QGeoTileSpec tile = *i;
        newTiles.insert(QGeoTileSpec(m_pluginString, m_mapType.mapId(), tile.zoom(), tile.x(), tile.y(), m_mapVersion));
    }

    m_tiles = newTiles;
}

void QGeoCameraTilesPrivate::updateGeometry()
{
    // Find the frustum from the camera / screen / viewport information
    // The larger frustum when stationary is a form of prefetching
    Frustum f = createFrustum(m_viewExpansion);

    // Find the polygon where the frustum intersects the plane of the map
    PolygonVector footprint = frustumFootprint(f);

    // Clip the polygon to the map, split it up if it cross the dateline
    QPair<PolygonVector, PolygonVector> polygons = clipFootprintToMap(footprint);

    if (!polygons.first.isEmpty()) {
        QSet<QGeoTileSpec> tilesLeft = tilesFromPolygon(polygons.first);
        m_tiles.unite(tilesLeft);
    }

    if (!polygons.second.isEmpty()) {
        QSet<QGeoTileSpec> tilesRight = tilesFromPolygon(polygons.second);
        m_tiles.unite(tilesRight);
    }
}

Frustum QGeoCameraTilesPrivate::createFrustum(double fieldOfViewGradient) const
{
    QDoubleVector3D center = m_sideLength * QGeoProjection::coordToMercator(m_camera.center());
    center.setZ(0.0);

    double f = qMin(m_screenSize.width(), m_screenSize.height());

    double z = std::pow(2.0, m_camera.zoomLevel() - m_intZoomLevel) * m_tileSize;

    double altitude = f / (2.0 * z);
    QDoubleVector3D eye = center;
    eye.setZ(altitude);

    QDoubleVector3D view = eye - center;
    QDoubleVector3D side = QDoubleVector3D::normal(view, QDoubleVector3D(0.0, 1.0, 0.0));
    QDoubleVector3D up = QDoubleVector3D::normal(side, view);

    double nearPlane =  1 / (4.0 * m_tileSize );
    double farPlane = altitude + 1.0;

    double aspectRatio = 1.0 * m_screenSize.width() / m_screenSize.height();

    double hn,wn,hf,wf = 0.0;

    // fixes field of view at 45 degrees
    // this assumes that viewSize = 2*nearPlane x 2*nearPlane

    if (aspectRatio > 1.0) {
        hn = 2 * fieldOfViewGradient * nearPlane;
        wn = hn * aspectRatio;

        hf = 2 * fieldOfViewGradient * farPlane;
        wf = hf * aspectRatio;
    } else {
        wn = 2 * fieldOfViewGradient * nearPlane;
        hn = wn / aspectRatio;

        wf = 2 * fieldOfViewGradient * farPlane;
        hf = wf / aspectRatio;
    }

    QDoubleVector3D d = center - eye;
    d.normalize();
    up.normalize();
    QDoubleVector3D right = QDoubleVector3D::normal(d, up);

    QDoubleVector3D cf = eye + d * farPlane;
    QDoubleVector3D cn = eye + d * nearPlane;

    Frustum frustum;

    frustum.topLeftFar = cf + (up * hf / 2) - (right * wf / 2);
    frustum.topRightFar = cf + (up * hf / 2) + (right * wf / 2);
    frustum.bottomLeftFar = cf - (up * hf / 2) - (right * wf / 2);
    frustum.bottomRightFar = cf - (up * hf / 2) + (right * wf / 2);

    frustum.topLeftNear = cn + (up * hn / 2) - (right * wn / 2);
    frustum.topRightNear = cn + (up * hn / 2) + (right * wn / 2);
    frustum.bottomLeftNear = cn - (up * hn / 2) - (right * wn / 2);
    frustum.bottomRightNear = cn - (up * hn / 2) + (right * wn / 2);

    return frustum;
}

void QGeoCameraTilesPrivate::appendZIntersects(const QDoubleVector3D &start,
                                               const QDoubleVector3D &end,
                                               double z,
                                               QVector<QDoubleVector3D> &results) const
{
    if (start.z() == end.z()) {
        if (start.z() == z) {
            results.append(start);
            results.append(end);
        }
    } else {
        double f = (start.z() - z) / (start.z() - end.z());
        if ((0 <= f) && (f <= 1.0)) {
            results.append((1 - f) * start + f * end);
        }
    }
}

/***************************************************/
/* Local copy of qSort & qSortHelper to suppress deprecation warnings
 * following the deprecation of QtAlgorithms. The comparison has subtle
 * differences which eluded detection so far. We just reuse old qSort for now.
 **/

template <typename RandomAccessIterator, typename LessThan>
inline void localqSort(RandomAccessIterator start, RandomAccessIterator end, LessThan lessThan)
{
    if (start != end)
        localqSortHelper(start, end, *start, lessThan);
}

template <typename RandomAccessIterator, typename T, typename LessThan>
void localqSortHelper(RandomAccessIterator start, RandomAccessIterator end, const T &t, LessThan lessThan)
{
top:
    int span = int(end - start);
    if (span < 2)
        return;

    --end;
    RandomAccessIterator low = start, high = end - 1;
    RandomAccessIterator pivot = start + span / 2;

    if (lessThan(*end, *start))
        qSwap(*end, *start);
    if (span == 2)
        return;

    if (lessThan(*pivot, *start))
        qSwap(*pivot, *start);
    if (lessThan(*end, *pivot))
        qSwap(*end, *pivot);
    if (span == 3)
        return;

    qSwap(*pivot, *end);

    while (low < high) {
        while (low < high && lessThan(*low, *end))
            ++low;

        while (high > low && lessThan(*end, *high))
            --high;

        if (low < high) {
            qSwap(*low, *high);
            ++low;
            --high;
        } else {
            break;
        }
    }

    if (lessThan(*low, *end))
        ++low;

    qSwap(*end, *low);
    localqSortHelper(start, low, t, lessThan);

    start = low + 1;
    ++end;
    goto top;
}
/***************************************************/


// Returns the intersection of the plane of the map and the camera frustum as a right handed polygon
PolygonVector QGeoCameraTilesPrivate::frustumFootprint(const Frustum &frustum) const
{
    PolygonVector points;
    points.reserve(24);

    appendZIntersects(frustum.topLeftNear, frustum.topLeftFar, 0.0, points);
    appendZIntersects(frustum.topRightNear, frustum.topRightFar, 0.0, points);
    appendZIntersects(frustum.bottomLeftNear, frustum.bottomLeftFar, 0.0, points);
    appendZIntersects(frustum.bottomRightNear, frustum.bottomRightFar, 0.0, points);

    appendZIntersects(frustum.topLeftNear, frustum.bottomLeftNear, 0.0, points);
    appendZIntersects(frustum.bottomLeftNear, frustum.bottomRightNear, 0.0, points);
    appendZIntersects(frustum.bottomRightNear, frustum.topRightNear, 0.0, points);
    appendZIntersects(frustum.topRightNear, frustum.topLeftNear, 0.0, points);

    appendZIntersects(frustum.topLeftFar, frustum.bottomLeftFar, 0.0, points);
    appendZIntersects(frustum.bottomLeftFar, frustum.bottomRightFar, 0.0, points);
    appendZIntersects(frustum.bottomRightFar, frustum.topRightFar, 0.0, points);
    appendZIntersects(frustum.topRightFar, frustum.topLeftFar, 0.0, points);

    if (points.isEmpty())
        return points;

    // sort points into a right handed polygon

    LengthSorter sorter;

    // - initial sort to remove duplicates
    sorter.base = points.first();
    localqSort(points.begin(), points.end(), sorter);
    //std::sort(points.begin(), points.end(), sorter);
    for (int i = points.size() - 1; i > 0; --i) {
        if (points.at(i) == points.at(i - 1))
            points.remove(i);
    }

    // - proper sort
    //   - start with the first point, put it in the sorted part of the list
    //   - add the nearest unsorted point to the last sorted point to the end
    //     of the sorted points
    PolygonVector::iterator i;
    for (i = points.begin(); i != points.end(); ++i) {
        sorter.base = *i;
        if (i + 1 != points.end())
            std::sort(i + 1, points.end(), sorter) ;
    }

    // - determine if what we have is right handed
    int size = points.size();
    if (size >= 3) {
        QDoubleVector3D normal = QDoubleVector3D::normal(points.at(1) - points.at(0),
                                                         points.at(2) - points.at(1));
        // - if not, reverse the list
        if (normal.z() < 0.0) {
            int halfSize = size / 2;
            for (int i = 0; i < halfSize; ++i) {
                QDoubleVector3D spare = points.at(i);
                points[i] = points[size - 1 - i];
                points[size - 1 - i] = spare;
            }
        }
    }

    return points;
}

QPair<PolygonVector, PolygonVector> QGeoCameraTilesPrivate::splitPolygonAtAxisValue(const PolygonVector &polygon, int axis, double value) const
{
    PolygonVector polygonBelow;
    PolygonVector polygonAbove;

    int size = polygon.size();

    if (size == 0) {
        return QPair<PolygonVector, PolygonVector>(polygonBelow, polygonAbove);
    }

    QVector<int> comparisons = QVector<int>(polygon.size());

    for (int i = 0; i < size; ++i) {
        double v = polygon.at(i).get(axis);
        if (qFuzzyCompare(v - value + 1.0, 1.0)) {
            comparisons[i] = 0;
        } else {
            if (v < value) {
                comparisons[i] = -1;
            } else if (value < v) {
                comparisons[i] = 1;
            }
        }
    }

    for (int index = 0; index < size; ++index) {
        int prevIndex = index - 1;
        if (prevIndex < 0)
            prevIndex += size;
        int nextIndex = (index + 1) % size;

        int prevComp = comparisons[prevIndex];
        int comp = comparisons[index];
        int nextComp = comparisons[nextIndex];

         if (comp == 0) {
            if (prevComp == -1) {
                polygonBelow.append(polygon.at(index));
                if (nextComp == 1) {
                    polygonAbove.append(polygon.at(index));
                }
            } else if (prevComp == 1) {
                polygonAbove.append(polygon.at(index));
                if (nextComp == -1) {
                    polygonBelow.append(polygon.at(index));
                }
            } else if (prevComp == 0) {
                if (nextComp == -1) {
                    polygonBelow.append(polygon.at(index));
                } else if (nextComp == 1) {
                    polygonAbove.append(polygon.at(index));
                } else if (nextComp == 0) {
                    // do nothing
                }
            }
        } else {
             if (comp == -1) {
                 polygonBelow.append(polygon.at(index));
             } else if (comp == 1) {
                 polygonAbove.append(polygon.at(index));
             }

             // there is a point between this and the next point
             // on the polygon that lies on the splitting line
             // and should be added to both the below and above
             // polygons
             if ((nextComp != 0) && (nextComp != comp)) {
                 QDoubleVector3D p1 = polygon.at(index);
                 QDoubleVector3D p2 = polygon.at(nextIndex);

                 double p1v = p1.get(axis);
                 double p2v = p2.get(axis);

                 double f = (p1v - value) / (p1v - p2v);

                 if (((0 <= f) && (f <= 1.0))
                         || qFuzzyCompare(f + 1.0, 1.0)
                         || qFuzzyCompare(f + 1.0, 2.0) ) {
                     QDoubleVector3D midPoint = (1.0 - f) * p1 + f * p2;
                     polygonBelow.append(midPoint);
                     polygonAbove.append(midPoint);
                 }
             }
        }
    }

    return QPair<PolygonVector, PolygonVector>(polygonBelow, polygonAbove);
}


QPair<PolygonVector, PolygonVector> QGeoCameraTilesPrivate::clipFootprintToMap(const PolygonVector &footprint) const
{
    bool clipX0 = false;
    bool clipX1 = false;
    bool clipY0 = false;
    bool clipY1 = false;

    double side = 1.0 * m_sideLength;

    typedef PolygonVector::const_iterator const_iter;

    const_iter i = footprint.constBegin();
    const_iter end = footprint.constEnd();
    for (; i != end; ++i) {
        QDoubleVector3D p = *i;
        if ((p.x() < 0.0) || (qFuzzyIsNull(p.x())))
            clipX0 = true;
        if ((side < p.x()) || (qFuzzyCompare(side, p.x())))
            clipX1 = true;
        if (p.y() < 0.0)
            clipY0 = true;
        if (side < p.y())
            clipY1 = true;
    }

    PolygonVector results = footprint;

    if (clipY0) {
        results = splitPolygonAtAxisValue(results, 1, 0.0).second;
    }

    if (clipY1) {
        results = splitPolygonAtAxisValue(results, 1, side).first;
    }

    if (clipX0) {
        if (clipX1) {
            results = splitPolygonAtAxisValue(results, 0, 0.0).second;
            results = splitPolygonAtAxisValue(results, 0, side).first;
            return QPair<PolygonVector, PolygonVector>(results, PolygonVector());
        } else {
            QPair<PolygonVector, PolygonVector> pair = splitPolygonAtAxisValue(results, 0, 0.0);
            if (pair.first.isEmpty()) {
                // if we touched the line but didn't cross it...
                for (int i = 0; i < pair.second.size(); ++i) {
                    if (qFuzzyIsNull(pair.second.at(i).x()))
                        pair.first.append(pair.second.at(i));
                }
                if (pair.first.size() == 2) {
                    double y0 = pair.first[0].y();
                    double y1 = pair.first[1].y();
                    pair.first.clear();
                    pair.first.append(QDoubleVector3D(side, y0, 0.0));
                    pair.first.append(QDoubleVector3D(side - 0.001, y0, 0.0));
                    pair.first.append(QDoubleVector3D(side - 0.001, y1, 0.0));
                    pair.first.append(QDoubleVector3D(side, y1, 0.0));
                } else if (pair.first.size() == 1) {
                    // FIXME this is trickier
                    // - touching at one point on the tile boundary
                    // - probably need to build a triangular polygon across the edge
                    // - don't want to add another y tile if we can help it
                    //   - initial version doesn't care
                    double y = pair.first.at(0).y();
                    pair.first.clear();
                    pair.first.append(QDoubleVector3D(side - 0.001, y, 0.0));
                    pair.first.append(QDoubleVector3D(side, y + 0.001, 0.0));
                    pair.first.append(QDoubleVector3D(side, y - 0.001, 0.0));
                }
            } else {
                for (int i = 0; i < pair.first.size(); ++i) {
                    pair.first[i].setX(pair.first.at(i).x() + side);
                }
            }
            return pair;
        }
    } else {
        if (clipX1) {
            QPair<PolygonVector, PolygonVector> pair = splitPolygonAtAxisValue(results, 0, side);
            if (pair.second.isEmpty()) {
                // if we touched the line but didn't cross it...
                for (int i = 0; i < pair.first.size(); ++i) {
                    if (qFuzzyCompare(side, pair.first.at(i).x()))
                        pair.second.append(pair.first.at(i));
                }
                if (pair.second.size() == 2) {
                    double y0 = pair.second[0].y();
                    double y1 = pair.second[1].y();
                    pair.second.clear();
                    pair.second.append(QDoubleVector3D(0, y0, 0.0));
                    pair.second.append(QDoubleVector3D(0.001, y0, 0.0));
                    pair.second.append(QDoubleVector3D(0.001, y1, 0.0));
                    pair.second.append(QDoubleVector3D(0, y1, 0.0));
                } else if (pair.second.size() == 1) {
                    // FIXME this is trickier
                    // - touching at one point on the tile boundary
                    // - probably need to build a triangular polygon across the edge
                    // - don't want to add another y tile if we can help it
                    //   - initial version doesn't care
                    double y = pair.second.at(0).y();
                    pair.second.clear();
                    pair.second.append(QDoubleVector3D(0.001, y, 0.0));
                    pair.second.append(QDoubleVector3D(0.0, y - 0.001, 0.0));
                    pair.second.append(QDoubleVector3D(0.0, y + 0.001, 0.0));
                }
            } else {
                for (int i = 0; i < pair.second.size(); ++i) {
                    pair.second[i].setX(pair.second.at(i).x() - side);
                }
            }
            return pair;
        } else {
            return QPair<PolygonVector, PolygonVector>(results, PolygonVector());
        }
    }

}

QList<QPair<double, int> > QGeoCameraTilesPrivate::tileIntersections(double p1, int t1, double p2, int t2) const
{
    if (t1 == t2) {
        QList<QPair<double, int> > results = QList<QPair<double, int> >();
        results.append(QPair<double, int>(0.0, t1));
        return results;
    }

    int step = 1;
    if (t1 > t2) {
        step = -1;
    }

    int size = 1 + ((t2 - t1) / step);

    QList<QPair<double, int> > results = QList<QPair<double, int> >();

    results.append(QPair<double, int>(0.0, t1));

    if (step == 1) {
        for (int i = 1; i < size; ++i) {
            double f = (t1 + i - p1) / (p2 - p1);
            results.append(QPair<double, int>(f, t1 + i));
        }
    } else {
        for (int i = 1; i < size; ++i) {
            double f = (t1 - i + 1 - p1) / (p2 - p1);
            results.append(QPair<double, int>(f, t1 - i));
        }
    }

    return results;
}

QSet<QGeoTileSpec> QGeoCameraTilesPrivate::tilesFromPolygon(const PolygonVector &polygon) const
{
    int numPoints = polygon.size();

    if (numPoints == 0)
        return QSet<QGeoTileSpec>();

    QVector<int> tilesX(polygon.size());
    QVector<int> tilesY(polygon.size());

    // grab tiles at the corners of the polygon
    for (int i = 0; i < numPoints; ++i) {

        QDoubleVector2D p = polygon.at(i).toVector2D();

        int x = 0;
        int y = 0;

        if (qFuzzyCompare(p.x(), m_sideLength * 1.0))
            x = m_sideLength - 1;
        else {
            x = static_cast<int>(p.x()) % m_sideLength;
            if ( !qFuzzyCompare(p.x(), 1.0 * x) && qFuzzyCompare(p.x(), 1.0 * (x + 1)) )
                x++;
        }

        if (qFuzzyCompare(p.y(), m_sideLength * 1.0))
            y = m_sideLength - 1;
        else {
            y = static_cast<int>(p.y()) % m_sideLength;
            if ( !qFuzzyCompare(p.y(), 1.0 * y) && qFuzzyCompare(p.y(), 1.0 * (y + 1)) )
                y++;
        }

        tilesX[i] = x;
        tilesY[i] = y;
    }

    QGeoCameraTilesPrivate::TileMap map;

    // walk along the edges of the polygon and add all tiles covered by them
    for (int i1 = 0; i1 < numPoints; ++i1) {
        int i2 = (i1 + 1) % numPoints;

        double x1 = polygon.at(i1).get(0);
        double x2 = polygon.at(i2).get(0);

        bool xFixed = qFuzzyCompare(x1, x2);
        bool xIntegral = qFuzzyCompare(x1, std::floor(x1)) || qFuzzyCompare(x1 + 1.0, std::floor(x1 + 1.0));

        QList<QPair<double, int> > xIntersects
                = tileIntersections(x1,
                                    tilesX.at(i1),
                                    x2,
                                    tilesX.at(i2));

        double y1 = polygon.at(i1).get(1);
        double y2 = polygon.at(i2).get(1);

        bool yFixed = qFuzzyCompare(y1, y2);
        bool yIntegral = qFuzzyCompare(y1, std::floor(y1)) || qFuzzyCompare(y1 + 1.0, std::floor(y1 + 1.0));

        QList<QPair<double, int> > yIntersects
                = tileIntersections(y1,
                                    tilesY.at(i1),
                                    y2,
                                    tilesY.at(i2));

        int x = xIntersects.takeFirst().second;
        int y = yIntersects.takeFirst().second;


        /*
          If the polygon coincides with the tile edges we must be
          inclusive and grab all tiles on both sides. We also need
          to handle tiles with corners coindent with the
          corners of the polygon.
          e.g. all tiles marked with 'x' will be added

              "+" - tile boundaries
              "O" - polygon boundary

                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +   x   +   x   +   x   +       +
                +       +       +       +       +       +
                + + + + + + + + O O O O O + + + + + + + +
                +       +       O       0       +       +
                +       +   x   O   x   0   x   +       +
                +       +       O       0       +       +
                + + + + + + + + O 0 0 0 0 + + + + + + + +
                +       +       +       +       +       +
                +       +   x   +   x   +   x   +       +
                +       +       +       +       +       +
                + + + + + + + + + + + + + + + + + + + + +
        */


        int xOther = x;
        int yOther = y;

        if (xFixed && xIntegral) {
             if (y2 < y1) {
                 xOther = qMax(0, x - 1);
            }
        }

        if (yFixed && yIntegral) {
            if (x1 < x2) {
                yOther = qMax(0, y - 1);

            }
        }

        if (xIntegral) {
            map.add(xOther, y);
            if (yIntegral)
                map.add(xOther, yOther);

        }

        if (yIntegral)
            map.add(x, yOther);

        map.add(x,y);

        // top left corner
        int iPrev =  (i1 + numPoints - 1) % numPoints;
        double xPrevious = polygon.at(iPrev).get(0);
        double yPrevious = polygon.at(iPrev).get(1);
        bool xPreviousFixed = qFuzzyCompare(xPrevious, x1);
        if (xIntegral && xPreviousFixed && yIntegral && yFixed) {
            if ((x2 > x1) && (yPrevious > y1)) {
                if ((x - 1) > 0 && (y - 1) > 0)
                    map.add(x - 1, y - 1);
            } else if ((x2 < x1) && (yPrevious < y1)) {
                // what?
            }
        }

        // for the simple case where intersections do not coincide with
        // the boundaries, we move along the edge and add tiles until
        // the x and y intersection lists are exhausted

        while (!xIntersects.isEmpty() && !yIntersects.isEmpty()) {
            QPair<double, int> nextX = xIntersects.first();
            QPair<double, int> nextY = yIntersects.first();
            if (nextX.first < nextY.first) {
                x = nextX.second;
                map.add(x, y);
                xIntersects.removeFirst();

            } else if (nextX.first > nextY.first) {
                y = nextY.second;
                map.add(x, y);
                yIntersects.removeFirst();

            } else {
                map.add(x, nextY.second);
                map.add(nextX.second, y);
                x = nextX.second;
                y = nextY.second;
                map.add(x, y);
                xIntersects.removeFirst();
                yIntersects.removeFirst();
            }
        }

        while (!xIntersects.isEmpty()) {
            x = xIntersects.takeFirst().second;
            map.add(x, y);
            if (yIntegral && yFixed)
                map.add(x, yOther);

        }

        while (!yIntersects.isEmpty()) {
            y = yIntersects.takeFirst().second;
            map.add(x, y);
            if (xIntegral && xFixed)
                map.add(xOther, y);
        }
    }

    QSet<QGeoTileSpec> results;

    int z = m_intZoomLevel;

    typedef QMap<int, QPair<int, int> >::const_iterator iter;
    iter i = map.data.constBegin();
    iter end = map.data.constEnd();

    for (; i != end; ++i) {
        int y = i.key();
        int minX = i->first;
        int maxX = i->second;
        for (int x = minX; x <= maxX; ++x) {
            results.insert(QGeoTileSpec(m_pluginString, m_mapType.mapId(), z, x, y, m_mapVersion));
        }
    }

    return results;
}

QGeoCameraTilesPrivate::TileMap::TileMap() {}

void QGeoCameraTilesPrivate::TileMap::add(int tileX, int tileY)
{
    if (data.contains(tileY)) {
        int oldMinX = data.value(tileY).first;
        int oldMaxX = data.value(tileY).second;
        data.insert(tileY, QPair<int, int>(qMin(tileX, oldMinX), qMax(tileX, oldMaxX)));
    } else {
        data.insert(tileY, QPair<int, int>(tileX, tileX));
    }
}

QT_END_NAMESPACE
