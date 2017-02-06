/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickpath_p.h"
#include "qquickpath_p_p.h"
#include "qquicksvgparser_p.h"

#include <QSet>
#include <QTime>

#include <private/qbezier_p.h>
#include <QtCore/qmath.h>
#include <QtCore/private/qnumeric_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype PathElement
    \instantiates QQuickPathElement
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief PathElement is the base path type

    This type is the base for all path types.  It cannot
    be instantiated.

    \sa Path, PathAttribute, PathPercent, PathLine, PathQuad, PathCubic, PathArc, PathCurve, PathSvg
*/

/*!
    \qmltype Path
    \instantiates QQuickPath
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Defines a path for use by \l PathView

    A Path is composed of one or more path segments - PathLine, PathQuad,
    PathCubic, PathArc, PathCurve, PathSvg.

    The spacing of the items along the Path can be adjusted via a
    PathPercent object.

    PathAttribute allows named attributes with values to be defined
    along the path.

    \sa PathView, PathAttribute, PathPercent, PathLine, PathQuad, PathCubic, PathArc, PathCurve, PathSvg
*/
QQuickPath::QQuickPath(QObject *parent)
 : QObject(*(new QQuickPathPrivate), parent)
{
}

QQuickPath::~QQuickPath()
{
}

/*!
    \qmlproperty real QtQuick::Path::startX
    \qmlproperty real QtQuick::Path::startY
    These properties hold the starting position of the path.
*/
qreal QQuickPath::startX() const
{
    Q_D(const QQuickPath);
    return d->startX.isNull ? 0 : d->startX.value;
}

void QQuickPath::setStartX(qreal x)
{
    Q_D(QQuickPath);
    if (d->startX.isValid() && qFuzzyCompare(x, d->startX))
        return;
    d->startX = x;
    emit startXChanged();
    processPath();
}

bool QQuickPath::hasStartX() const
{
    Q_D(const QQuickPath);
    return d->startX.isValid();
}

qreal QQuickPath::startY() const
{
    Q_D(const QQuickPath);
    return d->startY.isNull ? 0 : d->startY.value;
}

void QQuickPath::setStartY(qreal y)
{
    Q_D(QQuickPath);
    if (d->startY.isValid() && qFuzzyCompare(y, d->startY))
        return;
    d->startY = y;
    emit startYChanged();
    processPath();
}

bool QQuickPath::hasStartY() const
{
    Q_D(const QQuickPath);
    return d->startY.isValid();
}

/*!
    \qmlproperty bool QtQuick::Path::closed
    This property holds whether the start and end of the path are identical.
*/
bool QQuickPath::isClosed() const
{
    Q_D(const QQuickPath);
    return d->closed;
}

bool QQuickPath::hasEnd() const
{
    Q_D(const QQuickPath);
    for (int i = d->_pathElements.count() - 1; i > -1; --i) {
        if (QQuickCurve *curve = qobject_cast<QQuickCurve *>(d->_pathElements.at(i))) {
            if ((!curve->hasX() && !curve->hasRelativeX()) || (!curve->hasY() && !curve->hasRelativeY()))
                return false;
            else
                return true;
        }
    }
    return hasStartX() && hasStartY();
}

/*!
    \qmlproperty list<PathElement> QtQuick::Path::pathElements
    This property holds the objects composing the path.

    \default

    A path can contain the following path objects:
    \list
        \li \l PathLine - a straight line to a given position.
        \li \l PathQuad - a quadratic Bezier curve to a given position with a control point.
        \li \l PathCubic - a cubic Bezier curve to a given position with two control points.
        \li \l PathArc - an arc to a given position with a radius.
        \li \l PathSvg - a path specified as an SVG path data string.
        \li \l PathCurve - a point on a Catmull-Rom curve.
        \li \l PathAttribute - an attribute at a given position in the path.
        \li \l PathPercent - a way to spread out items along various segments of the path.
    \endlist

    \snippet qml/pathview/pathattributes.qml 2
*/

QQmlListProperty<QQuickPathElement> QQuickPath::pathElements()
{
    return QQmlListProperty<QQuickPathElement>(this,
                                               0,
                                               pathElements_append,
                                               pathElements_count,
                                               pathElements_at,
                                               pathElements_clear);
}

static QQuickPathPrivate *privatePath(QObject *object)
{
    QQuickPath *path = static_cast<QQuickPath*>(object);

    return QQuickPathPrivate::get(path);
}

QQuickPathElement *QQuickPath::pathElements_at(QQmlListProperty<QQuickPathElement> *property, int index)
{
    QQuickPathPrivate *d = privatePath(property->object);

    return d->_pathElements.at(index);
}

void QQuickPath::pathElements_append(QQmlListProperty<QQuickPathElement> *property, QQuickPathElement *pathElement)
{
    QQuickPathPrivate *d = privatePath(property->object);
    QQuickPath *path = static_cast<QQuickPath*>(property->object);

    d->_pathElements.append(pathElement);

    if (d->componentComplete) {
        QQuickCurve *curve = qobject_cast<QQuickCurve *>(pathElement);
        if (curve)
            d->_pathCurves.append(curve);
        else {
            QQuickPathAttribute *attribute = qobject_cast<QQuickPathAttribute *>(pathElement);
            if (attribute && !d->_attributes.contains(attribute->name()))
                d->_attributes.append(attribute->name());
        }

        path->processPath();

        connect(pathElement, SIGNAL(changed()), path, SLOT(processPath()));
    }
}

int QQuickPath::pathElements_count(QQmlListProperty<QQuickPathElement> *property)
{
    QQuickPathPrivate *d = privatePath(property->object);

    return d->_pathElements.count();
}

void QQuickPath::pathElements_clear(QQmlListProperty<QQuickPathElement> *property)
{
    QQuickPathPrivate *d = privatePath(property->object);
    QQuickPath *path = static_cast<QQuickPath*>(property->object);

    path->disconnectPathElements();
    d->_pathElements.clear();
    d->_pathCurves.clear();
    d->_pointCache.clear();
}

void QQuickPath::interpolate(int idx, const QString &name, qreal value)
{
    Q_D(QQuickPath);
    interpolate(d->_attributePoints, idx, name, value);
}

void QQuickPath::interpolate(QList<AttributePoint> &attributePoints, int idx, const QString &name, qreal value)
{
    if (!idx)
        return;

    qreal lastValue = 0;
    qreal lastPercent = 0;
    int search = idx - 1;
    while(search >= 0) {
        const AttributePoint &point = attributePoints.at(search);
        if (point.values.contains(name)) {
            lastValue = point.values.value(name);
            lastPercent = point.origpercent;
            break;
        }
        --search;
    }

    ++search;

    const AttributePoint &curPoint = attributePoints.at(idx);

    for (int ii = search; ii < idx; ++ii) {
        AttributePoint &point = attributePoints[ii];

        qreal val = lastValue + (value - lastValue) * (point.origpercent - lastPercent) / (curPoint.origpercent - lastPercent);
        point.values.insert(name, val);
    }
}

void QQuickPath::endpoint(const QString &name)
{
    Q_D(QQuickPath);
    const AttributePoint &first = d->_attributePoints.first();
    qreal val = first.values.value(name);
    for (int ii = d->_attributePoints.count() - 1; ii >= 0; ii--) {
        const AttributePoint &point = d->_attributePoints.at(ii);
        if (point.values.contains(name)) {
            for (int jj = ii + 1; jj < d->_attributePoints.count(); ++jj) {
                AttributePoint &setPoint = d->_attributePoints[jj];
                setPoint.values.insert(name, val);
            }
            return;
        }
    }
}

void QQuickPath::endpoint(QList<AttributePoint> &attributePoints, const QString &name)
{
    const AttributePoint &first = attributePoints.first();
    qreal val = first.values.value(name);
    for (int ii = attributePoints.count() - 1; ii >= 0; ii--) {
        const AttributePoint &point = attributePoints.at(ii);
        if (point.values.contains(name)) {
            for (int jj = ii + 1; jj < attributePoints.count(); ++jj) {
                AttributePoint &setPoint = attributePoints[jj];
                setPoint.values.insert(name, val);
            }
            return;
        }
    }
}

void QQuickPath::processPath()
{
    Q_D(QQuickPath);

    if (!d->componentComplete)
        return;

    d->_pointCache.clear();
    d->prevBez.isValid = false;

    d->_path = createPath(QPointF(), QPointF(), d->_attributes, d->pathLength, d->_attributePoints, &d->closed);

    emit changed();
}

QPainterPath QQuickPath::createPath(const QPointF &startPoint, const QPointF &endPoint, const QStringList &attributes, qreal &pathLength, QList<AttributePoint> &attributePoints, bool *closed)
{
    Q_D(QQuickPath);

    pathLength = 0;
    attributePoints.clear();

    if (!d->componentComplete)
        return QPainterPath();

    QPainterPath path;

    AttributePoint first;
    for (int ii = 0; ii < attributes.count(); ++ii)
        first.values[attributes.at(ii)] = 0;
    attributePoints << first;

    qreal startX = d->startX.isValid() ? d->startX.value : startPoint.x();
    qreal startY = d->startY.isValid() ? d->startY.value : startPoint.y();
    path.moveTo(startX, startY);

    const QString percentString = QStringLiteral("_qfx_percent");

    bool usesPercent = false;
    int index = 0;
    for (QQuickPathElement *pathElement : qAsConst(d->_pathElements)) {
        if (QQuickCurve *curve = qobject_cast<QQuickCurve *>(pathElement)) {
            QQuickPathData data;
            data.index = index;
            data.endPoint = endPoint;
            data.curves = d->_pathCurves;
            curve->addToPath(path, data);
            AttributePoint p;
            p.origpercent = path.length();
            attributePoints << p;
            ++index;
        } else if (QQuickPathAttribute *attribute = qobject_cast<QQuickPathAttribute *>(pathElement)) {
            AttributePoint &point = attributePoints.last();
            point.values[attribute->name()] = attribute->value();
            interpolate(attributePoints, attributePoints.count() - 1, attribute->name(), attribute->value());
        } else if (QQuickPathPercent *percent = qobject_cast<QQuickPathPercent *>(pathElement)) {
            AttributePoint &point = attributePoints.last();
            point.values[percentString] = percent->value();
            interpolate(attributePoints, attributePoints.count() - 1, percentString, percent->value());
            usesPercent = true;
        }
    }

    // Fixup end points
    const AttributePoint &last = attributePoints.constLast();
    for (int ii = 0; ii < attributes.count(); ++ii) {
        if (!last.values.contains(attributes.at(ii)))
            endpoint(attributePoints, attributes.at(ii));
    }
    if (usesPercent && !last.values.contains(percentString)) {
        d->_attributePoints.last().values[percentString] = 1;
        interpolate(d->_attributePoints.count() - 1, percentString, 1);
    }


    // Adjust percent
    qreal length = path.length();
    qreal prevpercent = 0;
    qreal prevorigpercent = 0;
    for (int ii = 0; ii < attributePoints.count(); ++ii) {
        const AttributePoint &point = attributePoints.at(ii);
        if (point.values.contains(percentString)) { //special string for QQuickPathPercent
            if ( ii > 0) {
                qreal scale = (attributePoints[ii].origpercent/length - prevorigpercent) /
                            (point.values.value(percentString)-prevpercent);
                attributePoints[ii].scale = scale;
            }
            attributePoints[ii].origpercent /= length;
            attributePoints[ii].percent = point.values.value(percentString);
            prevorigpercent = attributePoints.at(ii).origpercent;
            prevpercent = attributePoints.at(ii).percent;
        } else {
            attributePoints[ii].origpercent /= length;
            attributePoints[ii].percent = attributePoints.at(ii).origpercent;
        }
    }

    if (closed) {
        QPointF end = path.currentPosition();
        *closed = length > 0 && startX == end.x() && startY == end.y();
    }
    pathLength = length;

    return path;
}

void QQuickPath::classBegin()
{
    Q_D(QQuickPath);
    d->componentComplete = false;
}

void QQuickPath::disconnectPathElements()
{
    Q_D(const QQuickPath);

    for (QQuickPathElement *pathElement : d->_pathElements)
        disconnect(pathElement, SIGNAL(changed()), this, SLOT(processPath()));
}

void QQuickPath::connectPathElements()
{
    Q_D(const QQuickPath);

    for (QQuickPathElement *pathElement : d->_pathElements)
        connect(pathElement, SIGNAL(changed()), this, SLOT(processPath()));
}

void QQuickPath::gatherAttributes()
{
    Q_D(QQuickPath);

    QSet<QString> attributes;

    // First gather up all the attributes
    for (QQuickPathElement *pathElement : qAsConst(d->_pathElements)) {
        if (QQuickCurve *curve = qobject_cast<QQuickCurve *>(pathElement))
            d->_pathCurves.append(curve);
        else if (QQuickPathAttribute *attribute = qobject_cast<QQuickPathAttribute *>(pathElement))
            attributes.insert(attribute->name());
    }

    d->_attributes = attributes.toList();
}

void QQuickPath::componentComplete()
{
    Q_D(QQuickPath);
    d->componentComplete = true;

    gatherAttributes();

    processPath();

    connectPathElements();
}

QPainterPath QQuickPath::path() const
{
    Q_D(const QQuickPath);
    return d->_path;
}

QStringList QQuickPath::attributes() const
{
    Q_D(const QQuickPath);
    if (!d->componentComplete) {
        QSet<QString> attrs;

        // First gather up all the attributes
        for (QQuickPathElement *pathElement : d->_pathElements) {
            if (QQuickPathAttribute *attribute =
                qobject_cast<QQuickPathAttribute *>(pathElement))
                attrs.insert(attribute->name());
        }
        return attrs.toList();
    }
    return d->_attributes;
}

static inline QBezier nextBezier(const QPainterPath &path, int *current, qreal *bezLength, bool reverse = false)
{
    const int lastElement = reverse ? 0 : path.elementCount() - 1;
    const int start = reverse ? *current - 1 : *current + 1;
    for (int i=start; reverse ? i >= lastElement : i <= lastElement; reverse ? --i : ++i) {
        const QPainterPath::Element &e = path.elementAt(i);

        switch (e.type) {
        case QPainterPath::MoveToElement:
            break;
        case QPainterPath::LineToElement:
        {
            QLineF line(path.elementAt(i-1), e);
            *bezLength = line.length();
            QPointF a = path.elementAt(i-1);
            QPointF delta = e - a;
            *current = i;
            return QBezier::fromPoints(a, a + delta / 3, a + 2 * delta / 3, e);
        }
        case QPainterPath::CurveToElement:
        {
            QBezier b = QBezier::fromPoints(path.elementAt(i-1),
                                            e,
                                            path.elementAt(i+1),
                                            path.elementAt(i+2));
            *bezLength = b.length();
            *current = i;
            return b;
        }
        default:
            break;
        }
    }
    *current = lastElement;
    *bezLength = 0;
    return QBezier();
}

static inline int segmentCount(const QPainterPath &path, qreal pathLength)
{
    // In the really simple case of a single straight line we can interpolate without jitter
    // between just two points.
    if (path.elementCount() == 2
            && path.elementAt(0).type == QPainterPath::MoveToElement
            && path.elementAt(1).type == QPainterPath::LineToElement) {
        return 1;
    }
    // more points means less jitter between items as they move along the
    // path, but takes longer to generate
    return qCeil(pathLength*5);
}

//derivative of the equation
static inline qreal slopeAt(qreal t, qreal a, qreal b, qreal c, qreal d)
{
    return 3*t*t*(d - 3*c + 3*b - a) + 6*t*(c - 2*b + a) + 3*(b - a);
}

void QQuickPath::createPointCache() const
{
    Q_D(const QQuickPath);
    qreal pathLength = d->pathLength;
    if (pathLength <= 0 || qt_is_nan(pathLength))
        return;

    const int segments = segmentCount(d->_path, pathLength);
    const int lastElement = d->_path.elementCount() - 1;
    d->_pointCache.resize(segments+1);

    int currElement = -1;
    qreal bezLength = 0;
    QBezier currBez = nextBezier(d->_path, &currElement, &bezLength);
    qreal currLength = bezLength;
    qreal epc = currLength / pathLength;

    for (int i = 0; i < d->_pointCache.size(); i++) {
        //find which set we are in
        qreal prevPercent = 0;
        qreal prevOrigPercent = 0;
        for (int ii = 0; ii < d->_attributePoints.count(); ++ii) {
            qreal percent = qreal(i)/segments;
            const AttributePoint &point = d->_attributePoints.at(ii);
            if (percent < point.percent || ii == d->_attributePoints.count() - 1) { //### || is special case for very last item
                qreal elementPercent = (percent - prevPercent);

                qreal spc = prevOrigPercent + elementPercent * point.scale;

                while (spc > epc) {
                    if (currElement > lastElement)
                        break;
                    currBez = nextBezier(d->_path, &currElement, &bezLength);
                    if (bezLength == 0.0) {
                        currLength = pathLength;
                        epc = 1.0;
                        break;
                    }
                    currLength += bezLength;
                    epc = currLength / pathLength;
                }
                qreal realT = (pathLength * spc - (currLength - bezLength)) / bezLength;
                d->_pointCache[i] = currBez.pointAt(qBound(qreal(0), realT, qreal(1)));
                break;
            }
            prevOrigPercent = point.origpercent;
            prevPercent = point.percent;
        }
    }
}

void QQuickPath::invalidateSequentialHistory() const
{
    Q_D(const QQuickPath);
    d->prevBez.isValid = false;
}

QPointF QQuickPath::sequentialPointAt(qreal p, qreal *angle) const
{
    Q_D(const QQuickPath);
    return sequentialPointAt(d->_path, d->pathLength, d->_attributePoints, d->prevBez, p, angle);
}

QPointF QQuickPath::sequentialPointAt(const QPainterPath &path, const qreal &pathLength, const QList<AttributePoint> &attributePoints, QQuickCachedBezier &prevBez, qreal p, qreal *angle)
{
    Q_ASSERT(p >= 0.0 && p <= 1.0);

    if (!prevBez.isValid)
        return p > .5 ? backwardsPointAt(path, pathLength, attributePoints, prevBez, p, angle) :
                        forwardsPointAt(path, pathLength, attributePoints, prevBez, p, angle);

    return p < prevBez.p ? backwardsPointAt(path, pathLength, attributePoints, prevBez, p, angle) :
                           forwardsPointAt(path, pathLength, attributePoints, prevBez, p, angle);
}

QPointF QQuickPath::forwardsPointAt(const QPainterPath &path, const qreal &pathLength, const QList<AttributePoint> &attributePoints, QQuickCachedBezier &prevBez, qreal p, qreal *angle)
{
    if (pathLength <= 0 || qt_is_nan(pathLength))
        return path.pointAtPercent(0);  //expensive?

    const int lastElement = path.elementCount() - 1;
    bool haveCachedBez = prevBez.isValid;
    int currElement = haveCachedBez ? prevBez.element : -1;
    qreal bezLength = haveCachedBez ? prevBez.bezLength : 0;
    QBezier currBez = haveCachedBez ? prevBez.bezier : nextBezier(path, &currElement, &bezLength);
    qreal currLength = haveCachedBez ? prevBez.currLength : bezLength;
    qreal epc = currLength / pathLength;

    //find which set we are in
    qreal prevPercent = 0;
    qreal prevOrigPercent = 0;
    for (int ii = 0; ii < attributePoints.count(); ++ii) {
        qreal percent = p;
        const AttributePoint &point = attributePoints.at(ii);
        if (percent < point.percent || ii == attributePoints.count() - 1) {
            qreal elementPercent = (percent - prevPercent);

            qreal spc = prevOrigPercent + elementPercent * point.scale;

            while (spc > epc) {
                Q_ASSERT(!(currElement > lastElement));
                Q_UNUSED(lastElement);
                currBez = nextBezier(path, &currElement, &bezLength);
                currLength += bezLength;
                epc = currLength / pathLength;
            }
            prevBez.element = currElement;
            prevBez.bezLength = bezLength;
            prevBez.currLength = currLength;
            prevBez.bezier = currBez;
            prevBez.p = p;
            prevBez.isValid = true;

            qreal realT = (pathLength * spc - (currLength - bezLength)) / bezLength;

            if (angle) {
                qreal m1 = slopeAt(realT, currBez.x1, currBez.x2, currBez.x3, currBez.x4);
                qreal m2 = slopeAt(realT, currBez.y1, currBez.y2, currBez.y3, currBez.y4);
                *angle = QLineF(0, 0, m1, m2).angle();
            }

            return currBez.pointAt(qBound(qreal(0), realT, qreal(1)));
        }
        prevOrigPercent = point.origpercent;
        prevPercent = point.percent;
    }

    return QPointF(0,0);
}

//ideally this should be merged with forwardsPointAt
QPointF QQuickPath::backwardsPointAt(const QPainterPath &path, const qreal &pathLength, const QList<AttributePoint> &attributePoints, QQuickCachedBezier &prevBez, qreal p, qreal *angle)
{
    if (pathLength <= 0 || qt_is_nan(pathLength))
        return path.pointAtPercent(0);

    const int firstElement = 1; //element 0 is always a MoveTo, which we ignore
    bool haveCachedBez = prevBez.isValid;
    int currElement = haveCachedBez ? prevBez.element : path.elementCount();
    qreal bezLength = haveCachedBez ? prevBez.bezLength : 0;
    QBezier currBez = haveCachedBez ? prevBez.bezier : nextBezier(path, &currElement, &bezLength, true /*reverse*/);
    qreal currLength = haveCachedBez ? prevBez.currLength : pathLength;
    qreal prevLength = currLength - bezLength;
    qreal epc = prevLength / pathLength;

    for (int ii = attributePoints.count() - 1; ii > 0; --ii) {
        qreal percent = p;
        const AttributePoint &point = attributePoints.at(ii);
        const AttributePoint &prevPoint = attributePoints.at(ii-1);
        if (percent > prevPoint.percent || ii == 1) {
            qreal elementPercent = (percent - prevPoint.percent);

            qreal spc = prevPoint.origpercent + elementPercent * point.scale;

            while (spc < epc) {
                Q_ASSERT(!(currElement < firstElement));
                Q_UNUSED(firstElement);
                currBez = nextBezier(path, &currElement, &bezLength, true /*reverse*/);
                //special case for first element is to avoid floating point math
                //causing an epc that never hits 0.
                currLength = (currElement == firstElement) ? bezLength : prevLength;
                prevLength = currLength - bezLength;
                epc = prevLength / pathLength;
            }
            prevBez.element = currElement;
            prevBez.bezLength = bezLength;
            prevBez.currLength = currLength;
            prevBez.bezier = currBez;
            prevBez.p = p;
            prevBez.isValid = true;

            qreal realT = (pathLength * spc - (currLength - bezLength)) / bezLength;

            if (angle) {
                qreal m1 = slopeAt(realT, currBez.x1, currBez.x2, currBez.x3, currBez.x4);
                qreal m2 = slopeAt(realT, currBez.y1, currBez.y2, currBez.y3, currBez.y4);
                *angle = QLineF(0, 0, m1, m2).angle();
            }

            return currBez.pointAt(qBound(qreal(0), realT, qreal(1)));
        }
    }

    return QPointF(0,0);
}

QPointF QQuickPath::pointAt(qreal p) const
{
    Q_D(const QQuickPath);
    if (d->_pointCache.isEmpty()) {
        createPointCache();
        if (d->_pointCache.isEmpty())
            return QPointF();
    }

    const int segmentCount = d->_pointCache.size() - 1;
    qreal idxf = p*segmentCount;
    int idx1 = qFloor(idxf);
    qreal delta = idxf - idx1;
    if (idx1 > segmentCount)
        idx1 = segmentCount;
    else if (idx1 < 0)
        idx1 = 0;

    if (delta == 0.0)
        return d->_pointCache.at(idx1);

    // interpolate between the two points.
    int idx2 = qCeil(idxf);
    if (idx2 > segmentCount)
        idx2 = segmentCount;
    else if (idx2 < 0)
        idx2 = 0;

    QPointF p1 = d->_pointCache.at(idx1);
    QPointF p2 = d->_pointCache.at(idx2);
    QPointF pos = p1 * (1.0-delta) + p2 * delta;

    return pos;
}

qreal QQuickPath::attributeAt(const QString &name, qreal percent) const
{
    Q_D(const QQuickPath);
    if (percent < 0 || percent > 1)
        return 0;

    for (int ii = 0; ii < d->_attributePoints.count(); ++ii) {
        const AttributePoint &point = d->_attributePoints.at(ii);

        if (point.percent == percent) {
            return point.values.value(name);
        } else if (point.percent > percent) {
            qreal lastValue =
                ii?(d->_attributePoints.at(ii - 1).values.value(name)):0;
            qreal lastPercent =
                ii?(d->_attributePoints.at(ii - 1).percent):0;
            qreal curValue = point.values.value(name);
            qreal curPercent = point.percent;

            return lastValue + (curValue - lastValue) * (percent - lastPercent) / (curPercent - lastPercent);
        }
    }

    return 0;
}

/****************************************************************************/

qreal QQuickCurve::x() const
{
    return _x.isNull ? 0 : _x.value;
}

void QQuickCurve::setX(qreal x)
{
    if (_x.isNull || _x != x) {
        _x = x;
        emit xChanged();
        emit changed();
    }
}

bool QQuickCurve::hasX()
{
    return _x.isValid();
}

qreal QQuickCurve::y() const
{
    return _y.isNull ? 0 : _y.value;
}

void QQuickCurve::setY(qreal y)
{
    if (_y.isNull || _y != y) {
        _y = y;
        emit yChanged();
        emit changed();
    }
}

bool QQuickCurve::hasY()
{
    return _y.isValid();
}

qreal QQuickCurve::relativeX() const
{
    return _relativeX;
}

void QQuickCurve::setRelativeX(qreal x)
{
    if (_relativeX.isNull || _relativeX != x) {
        _relativeX = x;
        emit relativeXChanged();
        emit changed();
    }
}

bool QQuickCurve::hasRelativeX()
{
    return _relativeX.isValid();
}

qreal QQuickCurve::relativeY() const
{
    return _relativeY;
}

void QQuickCurve::setRelativeY(qreal y)
{
    if (_relativeY.isNull || _relativeY != y) {
        _relativeY = y;
        emit relativeYChanged();
        emit changed();
    }
}

bool QQuickCurve::hasRelativeY()
{
    return _relativeY.isValid();
}

/****************************************************************************/

/*!
    \qmltype PathAttribute
    \instantiates QQuickPathAttribute
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Specifies how to set an attribute at a given position in a Path

    The PathAttribute object allows attributes consisting of a name and
    a value to be specified for various points along a path.  The
    attributes are exposed to the delegate as
    \l{Attached Properties and Attached Signal Handlers} {Attached Properties}.
    The value of an attribute at any particular point along the path is interpolated
    from the PathAttributes bounding that point.

    The example below shows a path with the items scaled to 30% with
    opacity 50% at the top of the path and scaled 100% with opacity
    100% at the bottom.  Note the use of the PathView.iconScale and
    PathView.iconOpacity attached properties to set the scale and opacity
    of the delegate.

    \table
    \row
    \li \image declarative-pathattribute.png
    \li
    \snippet qml/pathview/pathattributes.qml 0
    (see the PathView documentation for the specification of ContactModel.qml
     used for ContactModel above.)
    \endtable


    \sa Path
*/

/*!
    \qmlproperty string QtQuick::PathAttribute::name
    This property holds the name of the attribute to change.

    This attribute will be available to the delegate as PathView.<name>

    Note that using an existing Item property name such as "opacity" as an
    attribute is allowed.  This is because path attributes add a new
    \l{Attached Properties and Attached Signal Handlers} {Attached Property}
    which in no way clashes with existing properties.
*/

/*!
    the name of the attribute to change.
*/

QString QQuickPathAttribute::name() const
{
    return _name;
}

void QQuickPathAttribute::setName(const QString &name)
{
    if (_name == name)
        return;
     _name = name;
    emit nameChanged();
}

/*!
   \qmlproperty real QtQuick::PathAttribute::value
   This property holds the value for the attribute.

   The value specified can be used to influence the visual appearance
   of an item along the path. For example, the following Path specifies
   an attribute named \e itemRotation, which has the value \e 0 at the
   beginning of the path, and the value 90 at the end of the path.

   \qml
   Path {
       startX: 0
       startY: 0
       PathAttribute { name: "itemRotation"; value: 0 }
       PathLine { x: 100; y: 100 }
       PathAttribute { name: "itemRotation"; value: 90 }
   }
   \endqml

   In our delegate, we can then bind the \e rotation property to the
   \l{Attached Properties and Attached Signal Handlers} {Attached Property}
   \e PathView.itemRotation created for this attribute.

   \qml
   Rectangle {
       width: 10; height: 10
       rotation: PathView.itemRotation
   }
   \endqml

   As each item is positioned along the path, it will be rotated accordingly:
   an item at the beginning of the path with be not be rotated, an item at
   the end of the path will be rotated 90 degrees, and an item mid-way along
   the path will be rotated 45 degrees.
*/

/*!
    the new value of the attribute.
*/
qreal QQuickPathAttribute::value() const
{
    return _value;
}

void QQuickPathAttribute::setValue(qreal value)
{
    if (_value != value) {
        _value = value;
        emit valueChanged();
        emit changed();
    }
}

/****************************************************************************/

/*!
    \qmltype PathLine
    \instantiates QQuickPathLine
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Defines a straight line

    The example below creates a path consisting of a straight line from
    0,100 to 200,100:

    \qml
    Path {
        startX: 0; startY: 100
        PathLine { x: 200; y: 100 }
    }
    \endqml

    \sa Path, PathQuad, PathCubic, PathArc, PathCurve, PathSvg
*/

/*!
    \qmlproperty real QtQuick::PathLine::x
    \qmlproperty real QtQuick::PathLine::y

    Defines the end point of the line.

    \sa relativeX, relativeY
*/

/*!
    \qmlproperty real QtQuick::PathLine::relativeX
    \qmlproperty real QtQuick::PathLine::relativeY

    Defines the end point of the line relative to its start.

    If both a relative and absolute end position are specified for a single axis, the relative
    position will be used.

    Relative and absolute positions can be mixed, for example it is valid to set a relative x
    and an absolute y.

    \sa x, y
*/

inline QPointF positionForCurve(const QQuickPathData &data, const QPointF &prevPoint)
{
    QQuickCurve *curve = data.curves.at(data.index);
    bool isEnd = data.index == data.curves.size() - 1;
    return QPointF(curve->hasRelativeX() ? prevPoint.x() + curve->relativeX() : !isEnd || curve->hasX() ? curve->x() : data.endPoint.x(),
                   curve->hasRelativeY() ? prevPoint.y() + curve->relativeY() : !isEnd || curve->hasY() ? curve->y() : data.endPoint.y());
}

void QQuickPathLine::addToPath(QPainterPath &path, const QQuickPathData &data)
{
    path.lineTo(positionForCurve(data, path.currentPosition()));
}

/****************************************************************************/

/*!
    \qmltype PathQuad
    \instantiates QQuickPathQuad
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Defines a quadratic Bezier curve with a control point

    The following QML produces the path shown below:
    \table
    \row
    \li \image declarative-pathquad.png
    \li
    \qml
    Path {
        startX: 0; startY: 0
        PathQuad { x: 200; y: 0; controlX: 100; controlY: 150 }
    }
    \endqml
    \endtable

    \sa Path, PathCubic, PathLine, PathArc, PathCurve, PathSvg
*/

/*!
    \qmlproperty real QtQuick::PathQuad::x
    \qmlproperty real QtQuick::PathQuad::y

    Defines the end point of the curve.

    \sa relativeX, relativeY
*/

/*!
    \qmlproperty real QtQuick::PathQuad::relativeX
    \qmlproperty real QtQuick::PathQuad::relativeY

    Defines the end point of the curve relative to its start.

    If both a relative and absolute end position are specified for a single axis, the relative
    position will be used.

    Relative and absolute positions can be mixed, for example it is valid to set a relative x
    and an absolute y.

    \sa x, y
*/

/*!
   \qmlproperty real QtQuick::PathQuad::controlX
   \qmlproperty real QtQuick::PathQuad::controlY

   Defines the position of the control point.
*/

/*!
    the x position of the control point.
*/
qreal QQuickPathQuad::controlX() const
{
    return _controlX;
}

void QQuickPathQuad::setControlX(qreal x)
{
    if (_controlX != x) {
        _controlX = x;
        emit controlXChanged();
        emit changed();
    }
}


/*!
    the y position of the control point.
*/
qreal QQuickPathQuad::controlY() const
{
    return _controlY;
}

void QQuickPathQuad::setControlY(qreal y)
{
    if (_controlY != y) {
        _controlY = y;
        emit controlYChanged();
        emit changed();
    }
}

/*!
   \qmlproperty real QtQuick::PathQuad::relativeControlX
   \qmlproperty real QtQuick::PathQuad::relativeControlY

    Defines the position of the control point relative to the curve's start.

    If both a relative and absolute control position are specified for a single axis, the relative
    position will be used.

    Relative and absolute positions can be mixed, for example it is valid to set a relative control x
    and an absolute control y.

    \sa controlX, controlY
*/

qreal QQuickPathQuad::relativeControlX() const
{
    return _relativeControlX;
}

void QQuickPathQuad::setRelativeControlX(qreal x)
{
    if (_relativeControlX.isNull || _relativeControlX != x) {
        _relativeControlX = x;
        emit relativeControlXChanged();
        emit changed();
    }
}

bool QQuickPathQuad::hasRelativeControlX()
{
    return _relativeControlX.isValid();
}

qreal QQuickPathQuad::relativeControlY() const
{
    return _relativeControlY;
}

void QQuickPathQuad::setRelativeControlY(qreal y)
{
    if (_relativeControlY.isNull || _relativeControlY != y) {
        _relativeControlY = y;
        emit relativeControlYChanged();
        emit changed();
    }
}

bool QQuickPathQuad::hasRelativeControlY()
{
    return _relativeControlY.isValid();
}

void QQuickPathQuad::addToPath(QPainterPath &path, const QQuickPathData &data)
{
    const QPointF &prevPoint = path.currentPosition();
    QPointF controlPoint(hasRelativeControlX() ? prevPoint.x() + relativeControlX() : controlX(),
                         hasRelativeControlY() ? prevPoint.y() + relativeControlY() : controlY());
    path.quadTo(controlPoint, positionForCurve(data, path.currentPosition()));
}

/****************************************************************************/

/*!
    \qmltype PathCubic
    \instantiates QQuickPathCubic
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Defines a cubic Bezier curve with two control points

    The following QML produces the path shown below:
    \table
    \row
    \li \image declarative-pathcubic.png
    \li
    \qml
    Path {
        startX: 20; startY: 0
        PathCubic {
            x: 180; y: 0
            control1X: -10; control1Y: 90
            control2X: 210; control2Y: 90
        }
    }
    \endqml
    \endtable

    \sa Path, PathQuad, PathLine, PathArc, PathCurve, PathSvg
*/

/*!
    \qmlproperty real QtQuick::PathCubic::x
    \qmlproperty real QtQuick::PathCubic::y

    Defines the end point of the curve.

    \sa relativeX, relativeY
*/

/*!
    \qmlproperty real QtQuick::PathCubic::relativeX
    \qmlproperty real QtQuick::PathCubic::relativeY

    Defines the end point of the curve relative to its start.

    If both a relative and absolute end position are specified for a single axis, the relative
    position will be used.

    Relative and absolute positions can be mixed, for example it is valid to set a relative x
    and an absolute y.

    \sa x, y
*/

/*!
   \qmlproperty real QtQuick::PathCubic::control1X
   \qmlproperty real QtQuick::PathCubic::control1Y

    Defines the position of the first control point.
*/
qreal QQuickPathCubic::control1X() const
{
    return _control1X;
}

void QQuickPathCubic::setControl1X(qreal x)
{
    if (_control1X != x) {
        _control1X = x;
        emit control1XChanged();
        emit changed();
    }
}

qreal QQuickPathCubic::control1Y() const
{
    return _control1Y;
}

void QQuickPathCubic::setControl1Y(qreal y)
{
    if (_control1Y != y) {
        _control1Y = y;
        emit control1YChanged();
        emit changed();
    }
}

/*!
   \qmlproperty real QtQuick::PathCubic::control2X
   \qmlproperty real QtQuick::PathCubic::control2Y

    Defines the position of the second control point.
*/
qreal QQuickPathCubic::control2X() const
{
    return _control2X;
}

void QQuickPathCubic::setControl2X(qreal x)
{
    if (_control2X != x) {
        _control2X = x;
        emit control2XChanged();
        emit changed();
    }
}

qreal QQuickPathCubic::control2Y() const
{
    return _control2Y;
}

void QQuickPathCubic::setControl2Y(qreal y)
{
    if (_control2Y != y) {
        _control2Y = y;
        emit control2YChanged();
        emit changed();
    }
}

/*!
   \qmlproperty real QtQuick::PathCubic::relativeControl1X
   \qmlproperty real QtQuick::PathCubic::relativeControl1Y
   \qmlproperty real QtQuick::PathCubic::relativeControl2X
   \qmlproperty real QtQuick::PathCubic::relativeControl2Y

    Defines the positions of the control points relative to the curve's start.

    If both a relative and absolute control position are specified for a control point's axis, the relative
    position will be used.

    Relative and absolute positions can be mixed, for example it is valid to set a relative control1 x
    and an absolute control1 y.

    \sa control1X, control1Y, control2X, control2Y
*/

qreal QQuickPathCubic::relativeControl1X() const
{
    return _relativeControl1X;
}

void QQuickPathCubic::setRelativeControl1X(qreal x)
{
    if (_relativeControl1X.isNull || _relativeControl1X != x) {
        _relativeControl1X = x;
        emit relativeControl1XChanged();
        emit changed();
    }
}

bool QQuickPathCubic::hasRelativeControl1X()
{
    return _relativeControl1X.isValid();
}

qreal QQuickPathCubic::relativeControl1Y() const
{
    return _relativeControl1Y;
}

void QQuickPathCubic::setRelativeControl1Y(qreal y)
{
    if (_relativeControl1Y.isNull || _relativeControl1Y != y) {
        _relativeControl1Y = y;
        emit relativeControl1YChanged();
        emit changed();
    }
}

bool QQuickPathCubic::hasRelativeControl1Y()
{
    return _relativeControl1Y.isValid();
}

qreal QQuickPathCubic::relativeControl2X() const
{
    return _relativeControl2X;
}

void QQuickPathCubic::setRelativeControl2X(qreal x)
{
    if (_relativeControl2X.isNull || _relativeControl2X != x) {
        _relativeControl2X = x;
        emit relativeControl2XChanged();
        emit changed();
    }
}

bool QQuickPathCubic::hasRelativeControl2X()
{
    return _relativeControl2X.isValid();
}

qreal QQuickPathCubic::relativeControl2Y() const
{
    return _relativeControl2Y;
}

void QQuickPathCubic::setRelativeControl2Y(qreal y)
{
    if (_relativeControl2Y.isNull || _relativeControl2Y != y) {
        _relativeControl2Y = y;
        emit relativeControl2YChanged();
        emit changed();
    }
}

bool QQuickPathCubic::hasRelativeControl2Y()
{
    return _relativeControl2Y.isValid();
}

void QQuickPathCubic::addToPath(QPainterPath &path, const QQuickPathData &data)
{
    const QPointF &prevPoint = path.currentPosition();
    QPointF controlPoint1(hasRelativeControl1X() ? prevPoint.x() + relativeControl1X() : control1X(),
                          hasRelativeControl1Y() ? prevPoint.y() + relativeControl1Y() : control1Y());
    QPointF controlPoint2(hasRelativeControl2X() ? prevPoint.x() + relativeControl2X() : control2X(),
                          hasRelativeControl2Y() ? prevPoint.y() + relativeControl2Y() : control2Y());
    path.cubicTo(controlPoint1, controlPoint2, positionForCurve(data, path.currentPosition()));
}

/****************************************************************************/

/*!
    \qmltype PathCurve
    \instantiates QQuickPathCatmullRomCurve
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Defines a point on a Catmull-Rom curve

    PathCurve provides an easy way to specify a curve passing directly through a set of points.
    Typically multiple PathCurves are used in a series, as the following example demonstrates:

    \snippet qml/path/basiccurve.qml 0

    This example produces the following path (with the starting point and PathCurve points
    highlighted in red):

    \image declarative-pathcurve.png

    \sa Path, PathLine, PathQuad, PathCubic, PathArc, PathSvg
*/

/*!
    \qmlproperty real QtQuick::PathCurve::x
    \qmlproperty real QtQuick::PathCurve::y

    Defines the end point of the curve.

    \sa relativeX, relativeY
*/

/*!
    \qmlproperty real QtQuick::PathCurve::relativeX
    \qmlproperty real QtQuick::PathCurve::relativeY

    Defines the end point of the curve relative to its start.

    If both a relative and absolute end position are specified for a single axis, the relative
    position will be used.

    Relative and absolute positions can be mixed, for example it is valid to set a relative x
    and an absolute y.

    \sa x, y
*/

inline QPointF previousPathPosition(const QPainterPath &path)
{
    int count = path.elementCount();
    if (count < 1)
        return QPointF();

    int index = path.elementAt(count-1).type == QPainterPath::CurveToDataElement ? count - 4 : count - 2;
    return index > -1 ? QPointF(path.elementAt(index)) : path.pointAtPercent(0);
}

void QQuickPathCatmullRomCurve::addToPath(QPainterPath &path, const QQuickPathData &data)
{
    //here we convert catmull-rom spline to bezier for use in QPainterPath.
    //basic conversion algorithm:
    //  catmull-rom points * inverse bezier matrix * catmull-rom matrix = bezier points
    //each point in the catmull-rom spline produces a bezier endpoint + 2 control points
    //calculations for each point use a moving window of 4 points
    //  (previous 2 points + current point + next point)
    QPointF prevFar, prev, point, next;

    //get previous points
    int index = data.index - 1;
    QQuickCurve *curve = index == -1 ? 0 : data.curves.at(index);
    if (qobject_cast<QQuickPathCatmullRomCurve*>(curve)) {
        prev = path.currentPosition();
        prevFar = previousPathPosition(path);
    } else {
        prev = path.currentPosition();
        bool prevFarSet = false;
        if (index == -1 && data.curves.count() > 1) {
            if (qobject_cast<QQuickPathCatmullRomCurve*>(data.curves.at(data.curves.count()-1))) {
                //TODO: profile and optimize
                QPointF pos = prev;
                QQuickPathData loopData;
                loopData.endPoint = data.endPoint;
                loopData.curves = data.curves;
                for (int i = data.index; i < data.curves.count(); ++i) {
                    loopData.index = i;
                    pos = positionForCurve(loopData, pos);
                    if (i == data.curves.count()-2)
                        prevFar = pos;
                }
                if (pos == QPointF(path.elementAt(0))) {
                    //this is a closed path starting and ending with catmull-rom segments.
                    //we try to smooth the join point
                    prevFarSet = true;
                }
            }
        }
        if (!prevFarSet)
            prevFar = prev;
    }

    //get current point
    point = positionForCurve(data, path.currentPosition());

    //get next point
    index = data.index + 1;
    if (index < data.curves.count() && qobject_cast<QQuickPathCatmullRomCurve*>(data.curves.at(index))) {
        QQuickPathData nextData;
        nextData.index = index;
        nextData.endPoint = data.endPoint;
        nextData.curves = data.curves;
        next = positionForCurve(nextData, point);
    } else {
        if (point == QPointF(path.elementAt(0)) && qobject_cast<QQuickPathCatmullRomCurve*>(data.curves.at(0)) && path.elementCount() >= 3) {
            //this is a closed path starting and ending with catmull-rom segments.
            //we try to smooth the join point
            next = QPointF(path.elementAt(3));  //the first catmull-rom point
        } else
            next = point;
    }

    /*
        full conversion matrix (inverse bezier * catmull-rom):
        0.000,  1.000,  0.000,  0.000,
        -0.167,  1.000,  0.167,  0.000,
        0.000,  0.167,  1.000, -0.167,
        0.000,  0.000,  1.000,  0.000

        conversion doesn't require full matrix multiplication,
        so below we simplify
    */
    QPointF control1(prevFar.x() * qreal(-0.167) +
                     prev.x() +
                     point.x() * qreal(0.167),
                     prevFar.y() * qreal(-0.167) +
                     prev.y() +
                     point.y() * qreal(0.167));

    QPointF control2(prev.x() * qreal(0.167) +
                     point.x() +
                     next.x() * qreal(-0.167),
                     prev.y() * qreal(0.167) +
                     point.y() +
                     next.y() * qreal(-0.167));

    path.cubicTo(control1, control2, point);
}

/****************************************************************************/

/*!
    \qmltype PathArc
    \instantiates QQuickPathArc
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Defines an arc with the given radius

    PathArc provides a simple way of specifying an arc that ends at a given position
    and uses the specified radius. It is modeled after the SVG elliptical arc command.

    The following QML produces the path shown below:
    \table
    \row
    \li \image declarative-patharc.png
    \li \snippet qml/path/basicarc.qml 0
    \endtable

    Note that a single PathArc cannot be used to specify a circle. Instead, you can
    use two PathArc elements, each specifying half of the circle.

    \sa Path, PathLine, PathQuad, PathCubic, PathCurve, PathSvg
*/

/*!
    \qmlproperty real QtQuick::PathArc::x
    \qmlproperty real QtQuick::PathArc::y

    Defines the end point of the arc.

    \sa relativeX, relativeY
*/

/*!
    \qmlproperty real QtQuick::PathArc::relativeX
    \qmlproperty real QtQuick::PathArc::relativeY

    Defines the end point of the arc relative to its start.

    If both a relative and absolute end position are specified for a single axis, the relative
    position will be used.

    Relative and absolute positions can be mixed, for example it is valid to set a relative x
    and an absolute y.

    \sa x, y
*/

/*!
    \qmlproperty real QtQuick::PathArc::radiusX
    \qmlproperty real QtQuick::PathArc::radiusY

    Defines the radius of the arc.

    The following QML demonstrates how different radius values can be used to change
    the shape of the arc:
    \table
    \row
    \li \image declarative-arcradius.png
    \li \snippet qml/path/arcradius.qml 0
    \endtable
*/

qreal QQuickPathArc::radiusX() const
{
    return _radiusX;
}

void QQuickPathArc::setRadiusX(qreal radius)
{
    if (_radiusX == radius)
        return;

    _radiusX = radius;
    emit radiusXChanged();
}

qreal QQuickPathArc::radiusY() const
{
    return _radiusY;
}

void QQuickPathArc::setRadiusY(qreal radius)
{
    if (_radiusY == radius)
        return;

    _radiusY = radius;
    emit radiusYChanged();
}

/*!
    \qmlproperty bool QtQuick::PathArc::useLargeArc
    Whether to use a large arc as defined by the arc points.

    Given fixed start and end positions, radius, and direction,
    there are two possible arcs that can fit the data. useLargeArc
    is used to distinguish between these. For example, the following
    QML can produce either of the two illustrated arcs below by
    changing the value of useLargeArc.

    \table
    \row
    \li \image declarative-largearc.png
    \li \snippet qml/path/largearc.qml 0
    \endtable

    The default value is false.
*/

bool QQuickPathArc::useLargeArc() const
{
    return _useLargeArc;
}

void QQuickPathArc::setUseLargeArc(bool largeArc)
{
    if (_useLargeArc == largeArc)
        return;

    _useLargeArc = largeArc;
    emit useLargeArcChanged();
}

/*!
    \qmlproperty enumeration QtQuick::PathArc::direction

    Defines the direction of the arc. Possible values are
    PathArc.Clockwise (default) and PathArc.Counterclockwise.

    The following QML can produce either of the two illustrated arcs below
    by changing the value of direction.
    \table
    \row
    \li \image declarative-arcdirection.png
    \li \snippet qml/path/arcdirection.qml 0
    \endtable

    \sa useLargeArc
*/

QQuickPathArc::ArcDirection QQuickPathArc::direction() const
{
    return _direction;
}

void QQuickPathArc::setDirection(ArcDirection direction)
{
    if (_direction == direction)
        return;

    _direction = direction;
    emit directionChanged();
}

void QQuickPathArc::addToPath(QPainterPath &path, const QQuickPathData &data)
{
    const QPointF &startPoint = path.currentPosition();
    const QPointF &endPoint = positionForCurve(data, startPoint);
    QQuickSvgParser::pathArc(path,
            _radiusX,
            _radiusY,
            0,  //xAxisRotation
            _useLargeArc,
            _direction == Clockwise ? 1 : 0,
            endPoint.x(),
            endPoint.y(),
            startPoint.x(), startPoint.y());
}

/****************************************************************************/

/*!
    \qmltype PathSvg
    \instantiates QQuickPathSvg
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Defines a path using an SVG path data string

    The following QML produces the path shown below:
    \table
    \row
    \li \image declarative-pathsvg.png
    \li
    \qml
    Path {
        startX: 50; startY: 50
        PathSvg { path: "L 150 50 L 100 150 z" }
    }
    \endqml
    \endtable

    \sa Path, PathLine, PathQuad, PathCubic, PathArc, PathCurve
*/

/*!
    \qmlproperty string QtQuick::PathSvg::path

    The SVG path data string specifying the path.

    See \l {http://www.w3.org/TR/SVG/paths.html#PathData}{W3C SVG Path Data}
    for more details on this format.
*/

QString QQuickPathSvg::path() const
{
    return _path;
}

void QQuickPathSvg::setPath(const QString &path)
{
    if (_path == path)
        return;

    _path = path;
    emit pathChanged();
}

void QQuickPathSvg::addToPath(QPainterPath &path, const QQuickPathData &)
{
    QQuickSvgParser::parsePathDataFast(_path, path);
}

/****************************************************************************/

/*!
    \qmltype PathPercent
    \instantiates QQuickPathPercent
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-paths
    \brief Manipulates the way a path is interpreted

    PathPercent allows you to manipulate the spacing between items on a
    PathView's path. You can use it to bunch together items on part of
    the path, and spread them out on other parts of the path.

    The examples below show the normal distribution of items along a path
    compared to a distribution which places 50% of the items along the
    PathLine section of the path.
    \table
    \row
    \li \image declarative-nopercent.png
    \li
    \qml
    PathView {
        // ...
        Path {
            startX: 20; startY: 0
            PathQuad { x: 50; y: 80; controlX: 0; controlY: 80 }
            PathLine { x: 150; y: 80 }
            PathQuad { x: 180; y: 0; controlX: 200; controlY: 80 }
        }
    }
    \endqml
    \row
    \li \image declarative-percent.png
    \li
    \qml
    PathView {
        // ...
        Path {
            startX: 20; startY: 0
            PathQuad { x: 50; y: 80; controlX: 0; controlY: 80 }
            PathPercent { value: 0.25 }
            PathLine { x: 150; y: 80 }
            PathPercent { value: 0.75 }
            PathQuad { x: 180; y: 0; controlX: 200; controlY: 80 }
            PathPercent { value: 1 }
        }
    }
    \endqml
    \endtable

    \sa Path
*/

/*!
    \qmlproperty real QtQuick::PathPercent::value
    The proportion of items that should be laid out up to this point.

    This value should always be higher than the last value specified
    by a PathPercent at a previous position in the Path.

    In the following example we have a Path made up of three PathLines.
    Normally, the items of the PathView would be laid out equally along
    this path, with an equal number of items per line segment. PathPercent
    allows us to specify that the first and third lines should each hold
    10% of the laid out items, while the second line should hold the remaining
    80%.

    \qml
    PathView {
        // ...
        Path {
            startX: 0; startY: 0
            PathLine { x:100; y: 0; }
            PathPercent { value: 0.1 }
            PathLine { x: 100; y: 100 }
            PathPercent { value: 0.9 }
            PathLine { x: 100; y: 0 }
            PathPercent { value: 1 }
        }
    }
    \endqml
*/

qreal QQuickPathPercent::value() const
{
    return _value;
}

void QQuickPathPercent::setValue(qreal value)
{
    if (_value != value) {
        _value = value;
        emit valueChanged();
        emit changed();
    }
}
QT_END_NAMESPACE
