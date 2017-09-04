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

#include <private/linechartitem_p.h>
#include <QtCharts/QLineSeries>
#include <private/qlineseries_p.h>
#include <private/chartpresenter_p.h>
#include <private/polardomain_p.h>
#include <private/chartthememanager_p.h>
#include <private/charttheme_p.h>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneMouseEvent>

QT_CHARTS_BEGIN_NAMESPACE

LineChartItem::LineChartItem(QLineSeries *series, QGraphicsItem *item)
    : XYChart(series,item),
      m_series(series),
      m_pointsVisible(false),
      m_chartType(QChart::ChartTypeUndefined),
      m_pointLabelsVisible(false),
      m_pointLabelsFormat(series->pointLabelsFormat()),
      m_pointLabelsFont(series->pointLabelsFont()),
      m_pointLabelsColor(series->pointLabelsColor()),
      m_pointLabelsClipping(true),
      m_mousePressed(false)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setZValue(ChartPresenter::LineChartZValue);
    QObject::connect(series->d_func(), SIGNAL(updated()), this, SLOT(handleUpdated()));
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

QRectF LineChartItem::boundingRect() const
{
    return m_rect;
}

QPainterPath LineChartItem::shape() const
{
    return m_shapePath;
}

void LineChartItem::updateGeometry()
{
    if (m_series->useOpenGL()) {
        if (!m_rect.isEmpty()) {
            prepareGeometryChange();
            // Changed signal seems to trigger even with empty region
            m_rect = QRectF();
        }
        update();
        return;
    }

    // Store the points to a local variable so that the old line gets properly cleared
    // when animation starts.
    m_linePoints = geometryPoints();
    const QVector<QPointF> &points = m_linePoints;

    if (points.size() == 0) {
        prepareGeometryChange();
        m_fullPath = QPainterPath();
        m_linePath = QPainterPath();
        m_rect = QRect();
        return;
    }

    QPainterPath linePath;
    QPainterPath fullPath;
    // Use worst case scenario to determine required margin.
    qreal margin = m_linePen.width() * 1.42;

    // Area series use component line series that aren't necessarily added to the chart themselves,
    // so check if chart type is forced before trying to obtain it from the chart.
    QChart::ChartType chartType = m_chartType;
    if (chartType == QChart::ChartTypeUndefined)
        chartType = m_series->chart()->chartType();

    // For polar charts, we need special handling for angular (horizontal)
    // points that are off-grid.
    if (chartType == QChart::ChartTypePolar) {
        QPainterPath linePathLeft;
        QPainterPath linePathRight;
        QPainterPath *currentSegmentPath = 0;
        QPainterPath *previousSegmentPath = 0;
        qreal minX = domain()->minX();
        qreal maxX = domain()->maxX();
        qreal minY = domain()->minY();
        QPointF currentSeriesPoint = m_series->at(0);
        QPointF currentGeometryPoint = points.at(0);
        QPointF previousGeometryPoint = points.at(0);
        int size = m_linePen.width();
        bool pointOffGrid = false;
        bool previousPointWasOffGrid = (currentSeriesPoint.x() < minX || currentSeriesPoint.x() > maxX);

        qreal domainRadius = domain()->size().height() / 2.0;
        const QPointF centerPoint(domainRadius, domainRadius);

        if (!previousPointWasOffGrid) {
            fullPath.moveTo(points.at(0));
            if (m_pointsVisible && currentSeriesPoint.y() >= minY) {
                // Do not draw ellipses for points below minimum Y.
                linePath.addEllipse(points.at(0), size, size);
                fullPath.addEllipse(points.at(0), size, size);
                linePath.moveTo(points.at(0));
                fullPath.moveTo(points.at(0));
            }
        }

        qreal leftMarginLine = centerPoint.x() - margin;
        qreal rightMarginLine = centerPoint.x() + margin;
        qreal horizontal = centerPoint.y();

        // See ScatterChartItem::updateGeometry() for explanation why seriesLastIndex is needed
        const int seriesLastIndex = m_series->count() - 1;

        for (int i = 1; i < points.size(); i++) {
            // Interpolating line fragments would be ugly when thick pen is used,
            // so we work around it by utilizing three separate
            // paths for line segments and clip those with custom regions at paint time.
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
                QPointF intersectionPoint;
                qreal y;
                if (pointOffGrid != previousPointWasOffGrid) {
                    if (currentGeometryPoint.x() == previousGeometryPoint.x()) {
                        y = currentGeometryPoint.y() + (currentGeometryPoint.y() - previousGeometryPoint.y()) / 2.0;
                    } else {
                        qreal ratio = (centerPoint.x() - currentGeometryPoint.x()) / (currentGeometryPoint.x() - previousGeometryPoint.x());
                        y = currentGeometryPoint.y() + (currentGeometryPoint.y() - previousGeometryPoint.y()) * ratio;
                    }
                    intersectionPoint = QPointF(centerPoint.x(), y);
                }

                bool dummyOk; // We know points are ok, but this is needed
                qreal currentAngle = 0;
                qreal previousAngle = 0;
                if (const PolarDomain *pd = qobject_cast<const PolarDomain *>(domain())) {
                    currentAngle = pd->toAngularCoordinate(currentSeriesPoint.x(), dummyOk);
                    previousAngle = pd->toAngularCoordinate(m_series->at(i - 1).x(), dummyOk);
                } else {
                    qWarning() << Q_FUNC_INFO << "Unexpected domain: " << domain();
                }
                if ((qAbs(currentAngle - previousAngle) > 180.0)) {
                    // If the angle between two points is over 180 degrees (half X range),
                    // any direct segment between them becomes meaningless.
                    // In this case two line segments are drawn instead, from previous
                    // point to the center and from center to current point.
                    if ((previousAngle < 0.0 || (previousAngle <= 180.0 && previousGeometryPoint.x() < rightMarginLine))
                        && previousGeometryPoint.y() < horizontal) {
                        currentSegmentPath = &linePathRight;
                    } else if ((previousAngle > 360.0 || (previousAngle > 180.0 && previousGeometryPoint.x() > leftMarginLine))
                                && previousGeometryPoint.y() < horizontal) {
                        currentSegmentPath = &linePathLeft;
                    } else if (previousAngle > 0.0 && previousAngle < 360.0) {
                        currentSegmentPath = &linePath;
                    } else {
                        currentSegmentPath = 0;
                    }

                    if (currentSegmentPath) {
                        if (previousSegmentPath != currentSegmentPath)
                            currentSegmentPath->moveTo(previousGeometryPoint);
                        if (previousPointWasOffGrid)
                            fullPath.moveTo(intersectionPoint);

                        currentSegmentPath->lineTo(centerPoint);
                        fullPath.lineTo(centerPoint);
                    }

                    previousSegmentPath = currentSegmentPath;

                    if ((currentAngle < 0.0 || (currentAngle <= 180.0 && currentGeometryPoint.x() < rightMarginLine))
                        && currentGeometryPoint.y() < horizontal) {
                        currentSegmentPath = &linePathRight;
                    } else if ((currentAngle > 360.0 || (currentAngle > 180.0 &&currentGeometryPoint.x() > leftMarginLine))
                                && currentGeometryPoint.y() < horizontal) {
                        currentSegmentPath = &linePathLeft;
                    } else if (currentAngle > 0.0 && currentAngle < 360.0) {
                        currentSegmentPath = &linePath;
                    } else {
                        currentSegmentPath = 0;
                    }

                    if (currentSegmentPath) {
                        if (previousSegmentPath != currentSegmentPath)
                            currentSegmentPath->moveTo(centerPoint);
                        if (!previousSegmentPath)
                            fullPath.moveTo(centerPoint);

                        currentSegmentPath->lineTo(currentGeometryPoint);
                        if (pointOffGrid)
                            fullPath.lineTo(intersectionPoint);
                        else
                            fullPath.lineTo(currentGeometryPoint);
                    }
                } else {
                    if (previousAngle < 0.0 || currentAngle < 0.0
                        || ((previousAngle <= 180.0 && currentAngle <= 180.0)
                            && ((previousGeometryPoint.x() < rightMarginLine && previousGeometryPoint.y() < horizontal)
                                || (currentGeometryPoint.x() < rightMarginLine && currentGeometryPoint.y() < horizontal)))) {
                        currentSegmentPath = &linePathRight;
                    } else if (previousAngle > 360.0 || currentAngle > 360.0
                               || ((previousAngle > 180.0 && currentAngle > 180.0)
                                   && ((previousGeometryPoint.x() > leftMarginLine && previousGeometryPoint.y() < horizontal)
                                       || (currentGeometryPoint.x() > leftMarginLine && currentGeometryPoint.y() < horizontal)))) {
                        currentSegmentPath = &linePathLeft;
                    } else {
                        currentSegmentPath = &linePath;
                    }

                    if (currentSegmentPath != previousSegmentPath)
                        currentSegmentPath->moveTo(previousGeometryPoint);
                    if (previousPointWasOffGrid)
                        fullPath.moveTo(intersectionPoint);

                    if (pointOffGrid)
                        fullPath.lineTo(intersectionPoint);
                    else
                        fullPath.lineTo(currentGeometryPoint);
                    currentSegmentPath->lineTo(currentGeometryPoint);
                }
            } else {
                currentSegmentPath = 0;
            }

            previousPointWasOffGrid = pointOffGrid;
            if (m_pointsVisible && !pointOffGrid && currentSeriesPoint.y() >= minY) {
                linePath.addEllipse(points.at(i), size, size);
                fullPath.addEllipse(points.at(i), size, size);
                linePath.moveTo(points.at(i));
                fullPath.moveTo(points.at(i));
            }
            previousSegmentPath = currentSegmentPath;
            previousGeometryPoint = currentGeometryPoint;
        }
        m_linePathPolarRight = linePathRight;
        m_linePathPolarLeft = linePathLeft;
        // Note: This construction of m_fullpath is not perfect. The partial segments that are
        // outside left/right clip regions at axis boundary still generate hover/click events,
        // because shape doesn't get clipped. It doesn't seem possible to do sensibly.
    } else { // not polar
        linePath.moveTo(points.at(0));
        if (m_pointsVisible) {
            int size = m_linePen.width();
            linePath.addEllipse(points.at(0), size, size);
            linePath.moveTo(points.at(0));
            for (int i = 1; i < points.size(); i++) {
                linePath.lineTo(points.at(i));
                linePath.addEllipse(points.at(i), size, size);
                linePath.moveTo(points.at(i));
            }
        } else {
            for (int i = 1; i < points.size(); i++)
                linePath.lineTo(points.at(i));
        }
        fullPath = linePath;
    }

    QPainterPathStroker stroker;
    // QPainter::drawLine does not respect join styles, for example BevelJoin becomes MiterJoin.
    // This is why we are prepared for the "worst case" scenario, i.e. use always MiterJoin and
    // multiply line width with square root of two when defining shape and bounding rectangle.
    stroker.setWidth(margin);
    stroker.setJoinStyle(Qt::MiterJoin);
    stroker.setCapStyle(Qt::SquareCap);
    stroker.setMiterLimit(m_linePen.miterLimit());

    QPainterPath checkShapePath = stroker.createStroke(fullPath);

    // Only zoom in if the bounding rects of the paths fit inside int limits. QWidget::update() uses
    // a region that has to be compatible with QRect.
    if (checkShapePath.boundingRect().height() <= INT_MAX
            && checkShapePath.boundingRect().width() <= INT_MAX
            && linePath.boundingRect().height() <= INT_MAX
            && linePath.boundingRect().width() <= INT_MAX
            && fullPath.boundingRect().height() <= INT_MAX
            && fullPath.boundingRect().width() <= INT_MAX) {
        prepareGeometryChange();

        m_linePath = linePath;
        m_fullPath = fullPath;
        m_shapePath = checkShapePath;

        m_rect = m_shapePath.boundingRect();
    } else {
        update();
    }
}

void LineChartItem::handleUpdated()
{
    // If points visibility has changed, a geometry update is needed.
    // Also, if pen changes when points are visible, geometry update is needed.
    bool doGeometryUpdate =
        (m_pointsVisible != m_series->pointsVisible())
        || (m_series->pointsVisible() && (m_linePen != m_series->pen()));
    bool visibleChanged = m_series->isVisible() != isVisible();
    setVisible(m_series->isVisible());
    setOpacity(m_series->opacity());
    m_pointsVisible = m_series->pointsVisible();
    m_linePen = m_series->pen();
    m_pointLabelsFormat = m_series->pointLabelsFormat();
    m_pointLabelsVisible = m_series->pointLabelsVisible();
    m_pointLabelsFont = m_series->pointLabelsFont();
    m_pointLabelsColor = m_series->pointLabelsColor();
    m_pointLabelsClipping = m_series->pointLabelsClipping();
    if (doGeometryUpdate)
        updateGeometry();
    else if (m_series->useOpenGL() && visibleChanged)
        refreshGlChart();
    update();
}

void LineChartItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)
    Q_UNUSED(option)

    if (m_series->useOpenGL())
        return;

    QRectF clipRect = QRectF(QPointF(0, 0), domain()->size());
    // Adjust clip rect half a pixel in required dimensions to make it include lines along the
    // plot area edges, but never increase clip so much that any portion of the line is draw beyond
    // the plot area.
    const qreal x1 = pos().x() - int(pos().x());
    const qreal y1 = pos().y() - int(pos().y());
    const qreal x2 = (clipRect.width() + 0.5) - int(clipRect.width() + 0.5);
    const qreal y2 = (clipRect.height() + 0.5) - int(clipRect.height() + 0.5);
    clipRect.adjust(-x1, -y1, qMax(x1, x2), qMax(y1, y2));

    painter->save();
    painter->setPen(m_linePen);
    bool alwaysUsePath = false;

    if (m_series->chart()->chartType() == QChart::ChartTypePolar) {
        qreal halfWidth = domain()->size().width() / 2.0;
        QRectF clipRectLeft = QRectF(0, 0, halfWidth, domain()->size().height());
        QRectF clipRectRight = QRectF(halfWidth, 0, halfWidth, domain()->size().height());
        QRegion fullPolarClipRegion(clipRect.toRect(), QRegion::Ellipse);
        QRegion clipRegionLeft(fullPolarClipRegion.intersected(clipRectLeft.toRect()));
        QRegion clipRegionRight(fullPolarClipRegion.intersected(clipRectRight.toRect()));
        painter->setClipRegion(clipRegionLeft);
        painter->drawPath(m_linePathPolarLeft);
        painter->setClipRegion(clipRegionRight);
        painter->drawPath(m_linePathPolarRight);
        painter->setClipRegion(fullPolarClipRegion);
        alwaysUsePath = true; // required for proper clipping
    } else {
        painter->setClipRect(clipRect);
    }

    if (m_pointsVisible) {
        painter->setBrush(m_linePen.color());
        painter->drawPath(m_linePath);
    } else {
        painter->setBrush(QBrush(Qt::NoBrush));
        if (m_linePen.style() != Qt::SolidLine || alwaysUsePath) {
            // If pen style is not solid line, always fall back to path painting
            // to ensure proper continuity of the pattern
            painter->drawPath(m_linePath);
        } else {
            for (int i(1); i < m_linePoints.size(); i++)
                painter->drawLine(m_linePoints.at(i - 1), m_linePoints.at(i));
        }
    }

    if (m_pointLabelsVisible) {
        if (m_pointLabelsClipping)
            painter->setClipping(true);
        else
            painter->setClipping(false);
        m_series->d_func()->drawSeriesPointLabels(painter, m_linePoints, m_linePen.width() / 2);
    }

    painter->restore();

}

void LineChartItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    emit XYChart::pressed(domain()->calculateDomainPoint(event->pos()));
    m_lastMousePos = event->pos();
    m_mousePressed = true;
    QGraphicsItem::mousePressEvent(event);
}

void LineChartItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    emit XYChart::hovered(domain()->calculateDomainPoint(event->pos()), true);
//    event->accept();
    QGraphicsItem::hoverEnterEvent(event);
}

void LineChartItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    emit XYChart::hovered(domain()->calculateDomainPoint(event->pos()), false);
//    event->accept();
    QGraphicsItem::hoverEnterEvent(event);
}

void LineChartItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit XYChart::released(domain()->calculateDomainPoint(m_lastMousePos));
    if (m_mousePressed)
        emit XYChart::clicked(domain()->calculateDomainPoint(m_lastMousePos));
    m_mousePressed = false;
    QGraphicsItem::mouseReleaseEvent(event);
}

void LineChartItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    emit XYChart::doubleClicked(domain()->calculateDomainPoint(m_lastMousePos));
    QGraphicsItem::mouseDoubleClickEvent(event);
}

#include "moc_linechartitem_p.cpp"

QT_CHARTS_END_NAMESPACE
