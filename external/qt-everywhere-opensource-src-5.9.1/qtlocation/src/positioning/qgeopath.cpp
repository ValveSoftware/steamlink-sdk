/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qgeopath.h"
#include "qgeopath_p.h"

#include "qgeocoordinate.h"
#include "qnumeric.h"
#include "qlocationutils_p.h"
#include "qwebmercator_p.h"

#include "qdoublevector2d_p.h"
#include "qdoublevector3d_p.h"
QT_BEGIN_NAMESPACE

/*!
    \class QGeoPath
    \inmodule QtPositioning
    \ingroup QtPositioning-positioning
    \since 5.9

    \brief The QGeoPath class defines a geographic path.

    The path is defined by an ordered list of QGeoCoordinates.

    Each two adjacent elements in the path are intended to be connected
    together by the shortest line segment of constant bearing passing
    through both elements.
    This type of connection can cross the dateline in the longitudinal direction,
    but never crosses the poles.

    This is relevant for the calculation of the bounding box returned by
    \l QGeoShape::boundingGeoRectangle() for this shape, which will have the latitude of
    the top left corner set to the maximum latitude in the path point set.
    Similarly, the latitude of the bottom right corner will be the minimum latitude
    in the path point set.

    This class is a \l Q_GADGET.
    It can be \l{Cpp_value_integration_positioning}{directly used from C++ and QML}.
*/

/*!
    \property QGeoPath::path
    \brief This property holds the list of coordinates for the geo path.

    The path is both invalid and empty if it contains no coordinate.

    A default constructed QGeoPath is therefore invalid.
*/

inline QGeoPathPrivate *QGeoPath::d_func()
{
    return static_cast<QGeoPathPrivate *>(d_ptr.data());
}

inline const QGeoPathPrivate *QGeoPath::d_func() const
{
    return static_cast<const QGeoPathPrivate *>(d_ptr.constData());
}

struct PathVariantConversions
{
    PathVariantConversions()
    {
        QMetaType::registerConverter<QGeoShape, QGeoPath>();
        QMetaType::registerConverter<QGeoPath, QGeoShape>();
    }
};

Q_GLOBAL_STATIC(PathVariantConversions, initPathConversions)

/*!
    Constructs a new, empty geo path.
*/
QGeoPath::QGeoPath()
:   QGeoShape(new QGeoPathPrivate)
{
    initPathConversions();
}

/*!
    Constructs a new geo path from a list of coordinates
    (\a path and \a width).
*/
QGeoPath::QGeoPath(const QList<QGeoCoordinate> &path, const qreal &width)
:   QGeoShape(new QGeoPathPrivate(path, width))
{
    initPathConversions();
}

/*!
    Constructs a new geo path from the contents of \a other.
*/
QGeoPath::QGeoPath(const QGeoPath &other)
:   QGeoShape(other)
{
    initPathConversions();
}

/*!
    Constructs a new geo path from the contents of \a other.
*/
QGeoPath::QGeoPath(const QGeoShape &other)
:   QGeoShape(other)
{
    initPathConversions();
    if (type() != QGeoShape::PathType)
        d_ptr = new QGeoPathPrivate;
}

/*!
    Destroys this path.
*/
QGeoPath::~QGeoPath() {}

/*!
    Assigns \a other to this geo path and returns a reference to this geo path.
*/
QGeoPath &QGeoPath::operator=(const QGeoPath &other)
{
    QGeoShape::operator=(other);
    return *this;
}

/*!
    Returns whether this geo path is equal to \a other.
*/
bool QGeoPath::operator==(const QGeoPath &other) const
{
    Q_D(const QGeoPath);
    return *d == *other.d_func();
}

/*!
    Returns whether this geo path is not equal to \a other.
*/
bool QGeoPath::operator!=(const QGeoPath &other) const
{
    Q_D(const QGeoPath);
    return !(*d == *other.d_func());
}

void QGeoPath::setPath(const QList<QGeoCoordinate> &path)
{
    Q_D(QGeoPath);
    return d->setPath(path);
}

/*!
    Returns all the elements. Equivalent to QGeoShape::center().
    The center coordinate, in case of a QGeoPath, is the center of its bounding box.
*/
const QList<QGeoCoordinate> &QGeoPath::path() const
{
    Q_D(const QGeoPath);
    return d->path();
}

void QGeoPath::setWidth(const qreal &width)
{
    Q_D(QGeoPath);
    d->setWidth(width);
}

/*!
    Returns the width of the path, in meters. This information is used in the \l contains method
    The default value is 0.
*/
qreal QGeoPath::width() const
{
    Q_D(const QGeoPath);
    return d->width();
}

/*!
    Translates this geo path by \a degreesLatitude northwards and \a degreesLongitude eastwards.

    Negative values of \a degreesLatitude and \a degreesLongitude correspond to
    southward and westward translation respectively.
*/
void QGeoPath::translate(double degreesLatitude, double degreesLongitude)
{
    Q_D(QGeoPath);
    d->translate(degreesLatitude, degreesLongitude);
}

/*!
    Returns a copy of this geo path translated by \a degreesLatitude northwards and
    \a degreesLongitude eastwards.

    Negative values of \a degreesLatitude and \a degreesLongitude correspond to
    southward and westward translation respectively.

    \sa translate()
*/
QGeoPath QGeoPath::translated(double degreesLatitude, double degreesLongitude) const
{
    QGeoPath result(*this);
    result.translate(degreesLatitude, degreesLongitude);
    return result;
}

/*!
    Returns the length of the path, in meters, from the element \a indexFrom to the element \a indexTo.
    The length is intended to be the sum of the shortest distances for each pair of adjacent points.
*/
double QGeoPath::length(int indexFrom, int indexTo) const
{
    Q_D(const QGeoPath);
    return d->length(indexFrom, indexTo);
}

/*!
    Appends \a coordinate to the path.
*/
void QGeoPath::addCoordinate(const QGeoCoordinate &coordinate)
{
    Q_D(QGeoPath);
    d->addCoordinate(coordinate);
}

/*!
    Inserts \a coordinate at the specified \a index.
*/
void QGeoPath::insertCoordinate(int index, const QGeoCoordinate &coordinate)
{
    Q_D(QGeoPath);
    d->insertCoordinate(index, coordinate);
}

/*!
    Replaces the path element at the specified \a index with \a coordinate.
*/
void QGeoPath::replaceCoordinate(int index, const QGeoCoordinate &coordinate)
{
    Q_D(QGeoPath);
    d->replaceCoordinate(index, coordinate);
}

/*!
    Returns the coordinate at \a index .
*/
QGeoCoordinate QGeoPath::coordinateAt(int index) const
{
    Q_D(const QGeoPath);
    return d->coordinateAt(index);
}

/*!
    Returns true if the path contains \a coordinate as one of the elements.
*/
bool QGeoPath::containsCoordinate(const QGeoCoordinate &coordinate) const
{
    Q_D(const QGeoPath);
    return d->containsCoordinate(coordinate);
}

/*!
    Removes the last occurrence of \a coordinate from the path.
*/
void QGeoPath::removeCoordinate(const QGeoCoordinate &coordinate)
{
    Q_D(QGeoPath);
    d->removeCoordinate(coordinate);
}

/*!
    Removes element at position \a index from the path.
*/
void QGeoPath::removeCoordinate(int index)
{
    Q_D(QGeoPath);
    d->removeCoordinate(index);
}

/*!
    Returns the geo path properties as a string.
*/
QString QGeoPath::toString() const
{
    if (type() != QGeoShape::PathType) {
        qWarning("Not a path");
        return QStringLiteral("QGeoPath(not a path)");
    }

    QString pathString;
    for (const auto &p : path())
        pathString += p.toString() + QLatin1Char(',');

    return QStringLiteral("QGeoPath([ %1 ])").arg(pathString);
}

/*******************************************************************************
 * QGeoPathPrivate
*******************************************************************************/

QGeoPathPrivate::QGeoPathPrivate()
:   QGeoShapePrivate(QGeoShape::PathType), m_width(0)
{
}

QGeoPathPrivate::QGeoPathPrivate(const QList<QGeoCoordinate> &path, const qreal width)
:   QGeoShapePrivate(QGeoShape::PathType), m_width(0)
{
    setPath(path);
    setWidth(width);
}

QGeoPathPrivate::QGeoPathPrivate(const QGeoPathPrivate &other)
:   QGeoShapePrivate(QGeoShape::PathType), m_path(other.m_path),
    m_deltaXs(other.m_deltaXs), m_minX(other.m_minX), m_maxX(other.m_maxX), m_minLati(other.m_minLati),
    m_maxLati(other.m_maxLati), m_bbox(other.m_bbox), m_width(other.m_width)
{
}

QGeoPathPrivate::~QGeoPathPrivate() {}

QGeoShapePrivate *QGeoPathPrivate::clone() const
{
    return new QGeoPathPrivate(*this);
}

bool QGeoPathPrivate::operator==(const QGeoShapePrivate &other) const
{
    if (!QGeoShapePrivate::operator==(other))
        return false;

    const QGeoPathPrivate &otherPath = static_cast<const QGeoPathPrivate &>(other);
    if (m_path.size() != otherPath.m_path.size())
        return false;
    return m_width == otherPath.m_width && m_path == otherPath.m_path;
}

bool QGeoPathPrivate::isValid() const
{
    return !isEmpty();
}

bool QGeoPathPrivate::isEmpty() const
{
    return m_path.isEmpty();
}

const QList<QGeoCoordinate> &QGeoPathPrivate::path() const
{
    return m_path;
}

void QGeoPathPrivate::setPath(const QList<QGeoCoordinate> &path)
{
    for (const QGeoCoordinate &c: path)
        if (!c.isValid())
            return;
    m_path = path;
    computeBoundingBox();
}

qreal QGeoPathPrivate::width() const
{
    return m_width;
}

void QGeoPathPrivate::setWidth(const qreal &width)
{
    if (qIsNaN(width) || width < 0.0)
        return;
    m_width = width;
}

double QGeoPathPrivate::length(int indexFrom, int indexTo) const
{
    if (indexTo < 0 || indexTo >= path().size())
        indexTo = path().size() - 1;
    double len = 0.0;
    // TODO: consider calculating the length of the actual rhumb line segments
    // instead of the shortest path from A to B.
    for (int i = indexFrom; i < indexTo; i++)
        len += m_path[i].distanceTo(m_path[i+1]);
    return len;
}

/*!
    Returns true if coordinate is present in m_path.
*/
bool QGeoPathPrivate::contains(const QGeoCoordinate &coordinate) const
{
    // Unoptimized approach:
    // - consider each segment of the path
    // - project it into mercator space (rhumb lines are straight in mercator space)
    // - find closest point to coordinate
    // - unproject the closest point
    // - calculate coordinate to closest point distance with distanceTo()
    // - if not within lineRadius, advance
    //
    // To keep wrapping into the equation:
    //   If the mercator x value of a coordinate of the line, or the coordinate parameter, is less
    // than mercator(m_bbox).x, add that to the conversion.

    double lineRadius = qMax(width() * 0.5, 0.2); // minimum radius: 20cm

    if (!m_path.size())
        return false;
    else if (m_path.size() == 1)
        return (m_path[0].distanceTo(coordinate) <= lineRadius);

    double leftBoundMercator = QWebMercator::coordToMercator(m_bbox.topLeft()).x();

    QDoubleVector2D p = QWebMercator::coordToMercator(coordinate);
    if (p.x() < leftBoundMercator)
        p.setX(p.x() + leftBoundMercator);  // unwrap X

    QDoubleVector2D a;
    QDoubleVector2D b;
    if (m_path.size()) {
        a = QWebMercator::coordToMercator(m_path[0]);
        if (a.x() < leftBoundMercator)
            a.setX(a.x() + leftBoundMercator);  // unwrap X
    }
    for (int i = 1; i < m_path.size(); i++) {
        b = QWebMercator::coordToMercator(m_path[i]);
        if (b.x() < leftBoundMercator)
            b.setX(b.x() + leftBoundMercator);  // unwrap X
        if (b == a)
            continue;

        double u = ((p.x() - a.x()) * (b.x() - a.x()) + (p.y() - a.y()) * (b.y() - a.y()) ) / (b - a).lengthSquared();
        QDoubleVector2D intersection(a.x() + u * (b.x() - a.x()) , a.y() + u * (b.y() - a.y()) );

        QDoubleVector2D candidate = ( (p-a).length() < (p-b).length() ) ? a : b;

        if (u > 0 && u < 1
            && (p-intersection).length() < (p-candidate).length()  ) // And it falls in the segment
                candidate = intersection;


        if (candidate.x() > 1.0)
            candidate.setX(candidate.x() - leftBoundMercator); // wrap X

        QGeoCoordinate closest = QWebMercator::mercatorToCoord(candidate);

        double distanceMeters = coordinate.distanceTo(closest);
        if (distanceMeters <= lineRadius)
            return true;

        // swap
        a = b;
    }

    // Last check if the coordinate is on the left of leftBoundMercator, but close enough to
    // m_path[0]
    return (m_path[0].distanceTo(coordinate) <= lineRadius);
}

QGeoCoordinate QGeoPathPrivate::center() const
{
    return boundingGeoRectangle().center();
}

QGeoRectangle QGeoPathPrivate::boundingGeoRectangle() const
{
    return m_bbox;
}

void QGeoPathPrivate::extendShape(const QGeoCoordinate &coordinate)
{
    if (!coordinate.isValid() || contains(coordinate))
        return;
    addCoordinate(coordinate);
}

void QGeoPathPrivate::translate(double degreesLatitude, double degreesLongitude)
{
    if (degreesLatitude > 0.0)
        degreesLatitude = qMin(degreesLatitude, 90.0 - m_maxLati);
    else
        degreesLatitude = qMax(degreesLatitude, -90.0 - m_minLati);
    for (QGeoCoordinate &p: m_path) {
        p.setLatitude(p.latitude() + degreesLatitude);
        p.setLongitude(QLocationUtils::wrapLong(p.longitude() + degreesLongitude));
    }
    m_bbox.translate(degreesLatitude, degreesLongitude);
    m_minLati += degreesLatitude;
    m_maxLati += degreesLatitude;
}

void QGeoPathPrivate::addCoordinate(const QGeoCoordinate &coordinate)
{
    if (!coordinate.isValid())
        return;
    m_path.append(coordinate);
    updateBoundingBox();
}

void QGeoPathPrivate::insertCoordinate(int index, const QGeoCoordinate &coordinate)
{
    if (index < 0 || index > m_path.size() || !coordinate.isValid())
        return;

    m_path.insert(index, coordinate);
    computeBoundingBox();
}

void QGeoPathPrivate::replaceCoordinate(int index, const QGeoCoordinate &coordinate)
{
    if (index < 0 || index >= m_path.size() || !coordinate.isValid())
        return;

    m_path[index] = coordinate;
    computeBoundingBox();
}

QGeoCoordinate QGeoPathPrivate::coordinateAt(int index) const
{
    if (index < 0 || index >= m_path.size())
        return QGeoCoordinate();

    return m_path.at(index);
}

bool QGeoPathPrivate::containsCoordinate(const QGeoCoordinate &coordinate) const
{
    return m_path.indexOf(coordinate) > -1;
}

void QGeoPathPrivate::removeCoordinate(const QGeoCoordinate &coordinate)
{
    int index = m_path.lastIndexOf(coordinate);
    removeCoordinate(index);
}

void QGeoPathPrivate::removeCoordinate(int index)
{
    if (index < 0 || index >= m_path.size())
        return;

    m_path.removeAt(index);
    computeBoundingBox();
}

void QGeoPathPrivate::computeBoundingBox()
{
    if (m_path.isEmpty()) {
        m_deltaXs.clear();
        m_minX = qInf();
        m_maxX = -qInf();
        m_minLati = qInf();
        m_maxLati = -qInf();
        m_bbox = QGeoRectangle();
        return;
    }

    m_minLati = m_maxLati = m_path.at(0).latitude();
    int minId = 0;
    int maxId = 0;
    m_deltaXs.resize(m_path.size());
    m_deltaXs[0] = m_minX = m_maxX = 0.0;

    for (int i = 1; i < m_path.size(); i++) {
        const QGeoCoordinate &geoFrom = m_path.at(i-1);
        const QGeoCoordinate &geoTo   = m_path.at(i);
        double longiFrom    = geoFrom.longitude();
        double longiTo      = geoTo.longitude();
        double deltaLongi = longiTo - longiFrom;
        if (qAbs(deltaLongi) > 180.0) {
            if (longiTo > 0.0)
                longiTo -= 360.0;
            else
                longiTo += 360.0;
            deltaLongi =  longiTo - longiFrom;
        }
        m_deltaXs[i] = m_deltaXs[i-1] + deltaLongi;
        if (m_deltaXs[i] < m_minX) {
            m_minX = m_deltaXs[i];
            minId = i;
        }
        if (m_deltaXs[i] > m_maxX) {
            m_maxX = m_deltaXs[i];
            maxId = i;
        }
        if (geoTo.latitude() > m_maxLati)
            m_maxLati = geoTo.latitude();
        if (geoTo.latitude() < m_minLati)
            m_minLati = geoTo.latitude();
    }

    m_bbox = QGeoRectangle(QGeoCoordinate(m_maxLati, m_path.at(minId).longitude()),
                           QGeoCoordinate(m_minLati, m_path.at(maxId).longitude()));
}

void QGeoPathPrivate::updateBoundingBox()
{
    if (m_path.isEmpty()) {
        m_deltaXs.clear();
        m_minX = qInf();
        m_maxX = -qInf();
        m_minLati = qInf();
        m_maxLati = -qInf();
        m_bbox = QGeoRectangle();
        return;
    } else if (m_path.size() == 1) { // was 0  now is 1
        m_deltaXs.resize(1);
        m_deltaXs[0] = m_minX = m_maxX = 0.0;
        m_minLati = m_maxLati = m_path.at(0).latitude();
        m_bbox = QGeoRectangle(QGeoCoordinate(m_maxLati, m_path.at(0).longitude()),
                               QGeoCoordinate(m_minLati, m_path.at(0).longitude()));
        return;
    } else if ( m_path.size() != m_deltaXs.size() + 1 ) {  // this case should not happen
        computeBoundingBox(); // something went wrong
        return;
    }

    const QGeoCoordinate &geoFrom = m_path.at(m_path.size()-2);
    const QGeoCoordinate &geoTo   = m_path.last();
    double longiFrom    = geoFrom.longitude();
    double longiTo      = geoTo.longitude();
    double deltaLongi = longiTo - longiFrom;
    if (qAbs(deltaLongi) > 180.0) {
        if (longiTo > 0.0)
            longiTo -= 360.0;
        else
            longiTo += 360.0;
        deltaLongi =  longiTo - longiFrom;
    }

    m_deltaXs.push_back(m_deltaXs.last() + deltaLongi);
    double currentMinLongi = m_bbox.topLeft().longitude();
    double currentMaxLongi = m_bbox.bottomRight().longitude();
    if (m_deltaXs.last() < m_minX) {
        m_minX = m_deltaXs.last();
        currentMinLongi = geoTo.longitude();
    }
    if (m_deltaXs.last() > m_maxX) {
        m_maxX = m_deltaXs.last();
        currentMaxLongi = geoTo.longitude();
    }
    if (geoTo.latitude() > m_maxLati)
        m_maxLati = geoTo.latitude();
    if (geoTo.latitude() < m_minLati)
        m_minLati = geoTo.latitude();
    m_bbox = QGeoRectangle(QGeoCoordinate(m_maxLati, currentMinLongi),
                           QGeoCoordinate(m_minLati, currentMaxLongi));
}

QT_END_NAMESPACE
