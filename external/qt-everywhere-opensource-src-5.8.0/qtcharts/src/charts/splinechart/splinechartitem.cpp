/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <private/splinechartitem_p.h>
#include <private/qsplineseries_p.h>
#include <private/chartpresenter_p.h>
#include <private/splineanimation_p.h>
#include <private/polardomain_p.h>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneMouseEvent>

QT_CHARTS_BEGIN_NAMESPACE

SplineChartItem::SplineChartItem(QSplineSeries *series, QGraphicsItem *item)
    : XYChart(series,item),
      m_series(series),
      m_pointsVisible(false),
      m_animation(0),
      m_pointLabelsVisible(false),
      m_pointLabelsFormat(series->pointLabelsFormat()),
      m_pointLabelsFont(series->pointLabelsFont()),
      m_pointLabelsColor(series->pointLabelsColor()),
      m_pointLabelsClipping(true),
      m_mousePressed(false)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setZValue(ChartPresenter::SplineChartZValue);
    QObject::connect(m_series->d_func(), SIGNAL(updated()), this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(visibleChanged()), this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(opacityChanged()), this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsFormatChanged(QString)),
                     this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsVisibilityChanged(bool)),
                     this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsFontChanged(QFont)), this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsColorChanged(QColor)), this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsClippingChanged(bool)), this, SLOT(handleUpdated()));
    handleUpdated();
}

QRectF SplineChartItem::boundingRect() const
{
    return m_rect;
}

QPainterPath SplineChartItem::shape() const
{
    return m_fullPath;
}

void SplineChartItem::setAnimation(SplineAnimation *animation)
{
    m_animation = animation;
    XYChart::setAnimation(animation);
}

ChartAnimation *SplineChartItem::animation() const
{
    return m_animation;
}

void SplineChartItem::setControlGeometryPoints(QVector<QPointF>& points)
{
    m_controlPoints = points;
}

QVector<QPointF> SplineChartItem::controlGeometryPoints() const
{
    return m_controlPoints;
}

void SplineChartItem::updateChart(QVector<QPointF> &oldPoints, QVector<QPointF> &newPoints, int index)
{
    QVector<QPointF> controlPoints;
    if (newPoints.count() >= 2)
        controlPoints = calculateControlPoints(newPoints);

    if (m_animation)
        m_animation->setup(oldPoints, newPoints, m_controlPoints, controlPoints, index);

    m_points = newPoints;
    m_controlPoints = controlPoints;
    setDirty(false);

    if (m_animation)
        presenter()->startAnimation(m_animation);
    else
        updateGeometry();
}

void SplineChartItem::updateGeometry()
{
    const QVector<QPointF> &points = m_points;
    const QVector<QPointF> &controlPoints = m_controlPoints;

    if ((points.size() < 2) || (controlPoints.size() < 2)) {
        prepareGeometryChange();
        m_path = QPainterPath();
        m_rect = QRect();
        return;
    }

    Q_ASSERT(points.count() * 2 - 2 == controlPoints.count());

    QPainterPath splinePath;
    QPainterPath fullPath;
    // Use worst case scenario to determine required margin.
    qreal margin = m_linePen.width() * 1.42;

    if (m_series->chart()->chartType() == QChart::ChartTypePolar) {
        QPainterPath splinePathLeft;
        QPainterPath splinePathRight;
        QPainterPath *currentSegmentPath = 0;
        QPainterPath *previousSegmentPath = 0;
        qreal minX = domain()->minX();
        qreal maxX = domain()->maxX();
        qreal minY = domain()->minY();
        QPointF currentSeriesPoint = m_series->at(0);
        QPointF currentGeometryPoint = points.at(0);
        QPointF previousGeometryPoint = points.at(0);
        bool pointOffGrid = false;
        bool previousPointWasOffGrid = (currentSeriesPoint.x() < minX || currentSeriesPoint.x() > maxX);
        m_visiblePoints.clear();
        m_visiblePoints.reserve(points.size());

        qreal domainRadius = domain()->size().height() / 2.0;
        const QPointF centerPoint(domainRadius, domainRadius);

        if (!previousPointWasOffGrid) {
            fullPath.moveTo(points.at(0));
            // Do not draw points for points below minimum Y.
            if (m_pointsVisible && currentSeriesPoint.y() >= minY)
                m_visiblePoints.append(currentGeometryPoint);
        }

        qreal leftMarginLine = centerPoint.x() - margin;
        qreal rightMarginLine = centerPoint.x() + margin;
        qreal horizontal = centerPoint.y();

        // See ScatterChartItem::updateGeometry() for explanation why seriesLastIndex is needed
        const int seriesLastIndex = m_series->count() - 1;

        for (int i = 1; i < points.size(); i++) {
            // Interpolating spline fragments accurately is not trivial, and would anyway be ugly
            // when thick pen is used, so we work around it by utilizing three separate
            // paths for spline segments and clip those with custom regions at paint time.
            // "Right" path contains segments that cross the axis line with visible point on the
            // right side of the axis line, as well as segments that have one point within the margin
            // on the right side of the axis line and another point on the right side of the chart.
            // "Left" path contains points with similarly on the left side.
            // "Full" path contains rest of the points.
            // This doesn't yield perfect results always. E.g. when segment covers more than 90
            // degrees and both of the points are within the margin, one in the top half and one in the
            // bottom half of the chart, the bottom one gets clipped incorrectly.
            // However, this should be rare occurrence in any sensible chart.
            currentSeriesPoint = m_series->at(qMin(seriesLastIndex, i));
            currentGeometryPoint = points.at(i);
            pointOffGrid = (currentSeriesPoint.x() < minX || currentSeriesPoint.x() > maxX);

            // Draw something unless both off-grid
            if (!pointOffGrid || !previousPointWasOffGrid) {
                bool dummyOk; // We know points are ok, but this is needed
                qreal currentAngle = static_cast<PolarDomain *>(domain())->toAngularCoordinate(currentSeriesPoint.x(), dummyOk);
                qreal previousAngle = static_cast<PolarDomain *>(domain())->toAngularCoordinate(m_series->at(i - 1).x(), dummyOk);

                if ((qAbs(currentAngle - previousAngle) > 180.0)) {
                    // If the angle between two points is over 180 degrees (half X range),
                    // any direct segment between them becomes meaningless.
                    // In this case two line segments are drawn instead, from previous
                    // point to the center and from center to current point.
                    if ((previousAngle < 0.0 || (previousAngle <= 180.0 && previousGeometryPoint.x() < rightMarginLine))
                        && previousGeometryPoint.y() < horizontal) {
                        currentSegmentPath = &splinePathRight;
                    } else if ((previousAngle > 360.0 || (previousAngle > 180.0 && previousGeometryPoint.x() > leftMarginLine))
                                && previousGeometryPoint.y() < horizontal) {
                        currentSegmentPath = &splinePathLeft;
                    } else if (previousAngle > 0.0 && previousAngle < 360.0) {
                        currentSegmentPath = &splinePath;
                    } else {
                        currentSegmentPath = 0;
                    }

                    if (currentSegmentPath) {
                        if (previousSegmentPath != currentSegmentPath)
                            currentSegmentPath->moveTo(previousGeometryPoint);
                        if (!previousSegmentPath)
                            fullPath.moveTo(previousGeometryPoint);

                        currentSegmentPath->lineTo(centerPoint);
                        fullPath.lineTo(centerPoint);
                    }

                    previousSegmentPath = currentSegmentPath;

                    if ((currentAngle < 0.0 || (currentAngle <= 180.0 && currentGeometryPoint.x() < rightMarginLine))
                        && currentGeometryPoint.y() < horizontal) {
                        currentSegmentPath = &splinePathRight;
                    } else if ((currentAngle > 360.0 || (currentAngle > 180.0 &&currentGeometryPoint.x() > leftMarginLine))
                                && currentGeometryPoint.y() < horizontal) {
                        currentSegmentPath = &splinePathLeft;
                    } else if (currentAngle > 0.0 && currentAngle < 360.0) {
                        currentSegmentPath = &splinePath;
                    } else {
                        currentSegmentPath = 0;
                    }

                    if (currentSegmentPath) {
                        if (previousSegmentPath != currentSegmentPath)
                            currentSegmentPath->moveTo(centerPoint);
                        if (!previousSegmentPath)
                            fullPath.moveTo(centerPoint);

                        currentSegmentPath->lineTo(currentGeometryPoint);
                        fullPath.lineTo(currentGeometryPoint);
                    }
                } else {
                    QPointF cp1 = controlPoints[2 * (i - 1)];
                    QPointF cp2 = controlPoints[(2 * i) - 1];

                    if (previousAngle < 0.0 || currentAngle < 0.0
                        || ((previousAngle <= 180.0 && currentAngle <= 180.0)
                            && ((previousGeometryPoint.x() < rightMarginLine && previousGeometryPoint.y() < horizontal)
                                || (currentGeometryPoint.x() < rightMarginLine && currentGeometryPoint.y() < horizontal)))) {
                        currentSegmentPath = &splinePathRight;
                    } else if (previousAngle > 360.0 || currentAngle > 360.0
                               || ((previousAngle > 180.0 && currentAngle > 180.0)
                                   && ((previousGeometryPoint.x() > leftMarginLine && previousGeometryPoint.y() < horizontal)
                                       || (currentGeometryPoint.x() > leftMarginLine && currentGeometryPoint.y() < horizontal)))) {
                        currentSegmentPath = &splinePathLeft;
                    } else {
                        currentSegmentPath = &splinePath;
                    }

                    if (currentSegmentPath != previousSegmentPath)
                        currentSegmentPath->moveTo(previousGeometryPoint);
                    if (!previousSegmentPath)
                        fullPath.moveTo(previousGeometryPoint);

                    fullPath.cubicTo(cp1, cp2, currentGeometryPoint);
                    currentSegmentPath->cubicTo(cp1, cp2, currentGeometryPoint);
                }
            } else {
                currentSegmentPath = 0;
            }

            previousPointWasOffGrid = pointOffGrid;
            if (!pointOffGrid && m_pointsVisible && currentSeriesPoint.y() >= minY)
                m_visiblePoints.append(currentGeometryPoint);
            previousSegmentPath = currentSegmentPath;
            previousGeometryPoint = currentGeometryPoint;
        }

        m_pathPolarRight = splinePathRight;
        m_pathPolarLeft = splinePathLeft;
        // Note: This construction of m_fullpath is not perfect. The partial segments that are
        // outside left/right clip regions at axis boundary still generate hover/click events,
        // because shape doesn't get clipped. It doesn't seem possible to do sensibly.
    } else { // not polar
        splinePath.moveTo(points.at(0));
        for (int i = 0; i < points.size() - 1; i++) {
            const QPointF &point = points.at(i + 1);
            splinePath.cubicTo(controlPoints[2 * i], controlPoints[2 * i + 1], point);
        }
        fullPath = splinePath;
    }

    QPainterPathStroker stroker;
    // The full path is comprised of three separate paths.
    // This is why we are prepared for the "worst case" scenario, i.e. use always MiterJoin and
    // multiply line width with square root of two when defining shape and bounding rectangle.
    stroker.setWidth(margin);
    stroker.setJoinStyle(Qt::MiterJoin);
    stroker.setCapStyle(Qt::SquareCap);
    stroker.setMiterLimit(m_linePen.miterLimit());

    // Only zoom in if the bounding rects of the path fit inside int limits. QWidget::update() uses
    // a region that has to be compatible with QRect.
    QPainterPath checkShapePath = stroker.createStroke(fullPath);
    if (checkShapePath.boundingRect().height() <= INT_MAX
            && checkShapePath.boundingRect().width() <= INT_MAX
            && splinePath.boundingRect().height() <= INT_MAX
            && splinePath.boundingRect().width() <= INT_MAX) {
        m_path = splinePath;

        prepareGeometryChange();

        m_fullPath = checkShapePath;
        m_rect = m_fullPath.boundingRect();
    }
}

/*!
  Calculates control points which are needed by QPainterPath.cubicTo function to draw the cubic Bezier cureve between two points.
  */
QVector<QPointF> SplineChartItem::calculateControlPoints(const QVector<QPointF> &points)
{
    QVector<QPointF> controlPoints;
    controlPoints.resize(points.count() * 2 - 2);

    int n = points.count() - 1;

    if (n == 1) {
        //for n==1
        controlPoints[0].setX((2 * points[0].x() + points[1].x()) / 3);
        controlPoints[0].setY((2 * points[0].y() + points[1].y()) / 3);
        controlPoints[1].setX(2 * controlPoints[0].x() - points[0].x());
        controlPoints[1].setY(2 * controlPoints[0].y() - points[0].y());
        return controlPoints;
    }

    // Calculate first Bezier control points
    // Set of equations for P0 to Pn points.
    //
    //  |   2   1   0   0   ... 0   0   0   ... 0   0   0   |   |   P1_1    |   |   P0 + 2 * P1             |
    //  |   1   4   1   0   ... 0   0   0   ... 0   0   0   |   |   P1_2    |   |   4 * P1 + 2 * P2         |
    //  |   0   1   4   1   ... 0   0   0   ... 0   0   0   |   |   P1_3    |   |   4 * P2 + 2 * P3         |
    //  |   .   .   .   .   .   .   .   .   .   .   .   .   |   |   ...     |   |   ...                     |
    //  |   0   0   0   0   ... 1   4   1   ... 0   0   0   | * |   P1_i    | = |   4 * P(i-1) + 2 * Pi     |
    //  |   .   .   .   .   .   .   .   .   .   .   .   .   |   |   ...     |   |   ...                     |
    //  |   0   0   0   0   0   0   0   0   ... 1   4   1   |   |   P1_(n-1)|   |   4 * P(n-2) + 2 * P(n-1) |
    //  |   0   0   0   0   0   0   0   0   ... 0   2   7   |   |   P1_n    |   |   8 * P(n-1) + Pn         |
    //
    QVector<qreal> vector;
    vector.resize(n);

    vector[0] = points[0].x() + 2 * points[1].x();


    for (int i = 1; i < n - 1; ++i)
        vector[i] = 4 * points[i].x() + 2 * points[i + 1].x();

    vector[n - 1] = (8 * points[n - 1].x() + points[n].x()) / 2.0;

    QVector<qreal> xControl = firstControlPoints(vector);

    vector[0] = points[0].y() + 2 * points[1].y();

    for (int i = 1; i < n - 1; ++i)
        vector[i] = 4 * points[i].y() + 2 * points[i + 1].y();

    vector[n - 1] = (8 * points[n - 1].y() + points[n].y()) / 2.0;

    QVector<qreal> yControl = firstControlPoints(vector);

    for (int i = 0, j = 0; i < n; ++i, ++j) {

        controlPoints[j].setX(xControl[i]);
        controlPoints[j].setY(yControl[i]);

        j++;

        if (i < n - 1) {
            controlPoints[j].setX(2 * points[i + 1].x() - xControl[i + 1]);
            controlPoints[j].setY(2 * points[i + 1].y() - yControl[i + 1]);
        } else {
            controlPoints[j].setX((points[n].x() + xControl[n - 1]) / 2);
            controlPoints[j].setY((points[n].y() + yControl[n - 1]) / 2);
        }
    }
    return controlPoints;
}

QVector<qreal> SplineChartItem::firstControlPoints(const QVector<qreal>& vector)
{
    QVector<qreal> result;

    int count = vector.count();
    result.resize(count);
    result[0] = vector[0] / 2.0;

    QVector<qreal> temp;
    temp.resize(count);
    temp[0] = 0;

    qreal b = 2.0;

    for (int i = 1; i < count; i++) {
        temp[i] = 1 / b;
        b = (i < count - 1 ? 4.0 : 3.5) - temp[i];
        result[i] = (vector[i] - result[i - 1]) / b;
    }

    for (int i = 1; i < count; i++)
        result[count - i - 1] -= temp[count - i] * result[count - i];

    return result;
}

//handlers

void SplineChartItem::handleUpdated()
{
    setVisible(m_series->isVisible());
    setOpacity(m_series->opacity());
    m_pointsVisible = m_series->pointsVisible();
    m_linePen = m_series->pen();
    m_pointPen = m_series->pen();
    m_pointPen.setWidthF(2 * m_pointPen.width());
    m_pointLabelsFormat = m_series->pointLabelsFormat();
    m_pointLabelsVisible = m_series->pointLabelsVisible();
    m_pointLabelsFont = m_series->pointLabelsFont();
    m_pointLabelsColor = m_series->pointLabelsColor();
    m_pointLabelsClipping = m_series->pointLabelsClipping();
    update();
}

//painter

void SplineChartItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)
    Q_UNUSED(option)

    QRectF clipRect = QRectF(QPointF(0, 0), domain()->size());

    painter->save();
    painter->setPen(m_linePen);
    painter->setBrush(Qt::NoBrush);

    if (m_series->chart()->chartType() == QChart::ChartTypePolar) {
        qreal halfWidth = domain()->size().width() / 2.0;
        QRectF clipRectLeft = QRectF(0, 0, halfWidth, domain()->size().height());
        QRectF clipRectRight = QRectF(halfWidth, 0, halfWidth, domain()->size().height());
        QRegion fullPolarClipRegion(clipRect.toRect(), QRegion::Ellipse);
        QRegion clipRegionLeft(fullPolarClipRegion.intersected(clipRectLeft.toRect()));
        QRegion clipRegionRight(fullPolarClipRegion.intersected(clipRectRight.toRect()));
        painter->setClipRegion(clipRegionLeft);
        painter->drawPath(m_pathPolarLeft);
        painter->setClipRegion(clipRegionRight);
        painter->drawPath(m_pathPolarRight);
        painter->setClipRegion(fullPolarClipRegion);
    } else {
        painter->setClipRect(clipRect);
    }

    painter->drawPath(m_path);

    if (m_pointsVisible) {
        painter->setPen(m_pointPen);
        if (m_series->chart()->chartType() == QChart::ChartTypePolar)
            painter->drawPoints(m_visiblePoints);
        else
            painter->drawPoints(geometryPoints());
    }

    if (m_pointLabelsVisible) {
        if (m_pointLabelsClipping)
            painter->setClipping(true);
        else
            painter->setClipping(false);
        m_series->d_func()->drawSeriesPointLabels(painter, m_points, m_linePen.width() / 2);
    }

    painter->restore();
}

void SplineChartItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    emit XYChart::pressed(domain()->calculateDomainPoint(event->pos()));
    m_lastMousePos = event->pos();
    m_mousePressed = true;
    QGraphicsItem::mousePressEvent(event);
}

void SplineChartItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    emit XYChart::hovered(domain()->calculateDomainPoint(event->pos()), true);
    QGraphicsItem::hoverEnterEvent(event);
}

void SplineChartItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    emit XYChart::hovered(domain()->calculateDomainPoint(event->pos()), false);
    QGraphicsItem::hoverLeaveEvent(event);
}

void SplineChartItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit XYChart::released(domain()->calculateDomainPoint(m_lastMousePos));
    if (m_mousePressed)
        emit XYChart::clicked(domain()->calculateDomainPoint(m_lastMousePos));
    m_mousePressed = false;
    QGraphicsItem::mouseReleaseEvent(event);
}

void SplineChartItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    emit XYChart::doubleClicked(domain()->calculateDomainPoint(m_lastMousePos));
    QGraphicsItem::mouseDoubleClickEvent(event);
}

#include "moc_splinechartitem_p.cpp"

QT_CHARTS_END_NAMESPACE
