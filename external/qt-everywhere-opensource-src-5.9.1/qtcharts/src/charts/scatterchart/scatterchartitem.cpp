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

#include <private/scatterchartitem_p.h>
#include <QtCharts/QScatterSeries>
#include <private/qscatterseries_p.h>
#include <private/chartpresenter_p.h>
#include <private/abstractdomain_p.h>
#include <QtCharts/QChart>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsScene>
#include <QtCore/QDebug>
#include <QtWidgets/QGraphicsSceneMouseEvent>

QT_CHARTS_BEGIN_NAMESPACE

ScatterChartItem::ScatterChartItem(QScatterSeries *series, QGraphicsItem *item)
    : XYChart(series,item),
      m_series(series),
      m_items(this),
      m_visible(true),
      m_shape(QScatterSeries::MarkerShapeRectangle),
      m_size(15),
      m_pointLabelsVisible(false),
      m_pointLabelsFormat(series->pointLabelsFormat()),
      m_pointLabelsFont(series->pointLabelsFont()),
      m_pointLabelsColor(series->pointLabelsColor()),
      m_pointLabelsClipping(true),
      m_mousePressed(false)
{
    QObject::connect(m_series->d_func(), SIGNAL(updated()), this, SLOT(handleUpdated()));
    QObject::connect(m_series, SIGNAL(visibleChanged()), this, SLOT(handleUpdated()));
    QObject::connect(m_series, SIGNAL(opacityChanged()), this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsFormatChanged(QString)),
                     this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsVisibilityChanged(bool)),
                     this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsFontChanged(QFont)), this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsColorChanged(QColor)), this, SLOT(handleUpdated()));
    QObject::connect(series, SIGNAL(pointLabelsClippingChanged(bool)), this, SLOT(handleUpdated()));

    setZValue(ChartPresenter::ScatterSeriesZValue);
    setFlags(QGraphicsItem::ItemClipsChildrenToShape);

    handleUpdated();

    m_items.setHandlesChildEvents(false);
}

QRectF ScatterChartItem::boundingRect() const
{
    return m_rect;
}

void ScatterChartItem::createPoints(int count)
{
    for (int i = 0; i < count; ++i) {

        QGraphicsItem *item = 0;

        switch (m_shape) {
        case QScatterSeries::MarkerShapeCircle: {
            item = new CircleMarker(0, 0, m_size, m_size, this);
            const QRectF &rect = item->boundingRect();
            item->setPos(-rect.width() / 2, -rect.height() / 2);
            break;
        }
        case QScatterSeries::MarkerShapeRectangle:
            item = new RectangleMarker(0, 0, m_size, m_size, this);
            item->setPos(-m_size / 2, -m_size / 2);
            break;
        default:
            qWarning() << "Unsupported marker type";
            break;
        }
        m_items.addToGroup(item);
    }
}

void ScatterChartItem::deletePoints(int count)
{
    QList<QGraphicsItem *> items = m_items.childItems();

    for (int i = 0; i < count; ++i) {
        QGraphicsItem *item = items.takeLast();
        m_markerMap.remove(item);
        delete(item);
    }
}

void ScatterChartItem::markerSelected(QGraphicsItem *marker)
{
    emit XYChart::clicked(m_markerMap[marker]);
}

void ScatterChartItem::markerHovered(QGraphicsItem *marker, bool state)
{
    emit XYChart::hovered(m_markerMap[marker], state);
}

void ScatterChartItem::markerPressed(QGraphicsItem *marker)
{
    emit XYChart::pressed(m_markerMap[marker]);
}

void ScatterChartItem::markerReleased(QGraphicsItem *marker)
{
    emit XYChart::released(m_markerMap[marker]);
}

void ScatterChartItem::markerDoubleClicked(QGraphicsItem *marker)
{
    emit XYChart::doubleClicked(m_markerMap[marker]);
}

void ScatterChartItem::updateGeometry()
{
    if (m_series->useOpenGL()) {
        if (m_items.childItems().count())
            deletePoints(m_items.childItems().count());
        if (!m_rect.isEmpty()) {
            prepareGeometryChange();
            // Changed signal seems to trigger even with empty region
            m_rect = QRectF();
        }
        update();
        return;
    }

    const QVector<QPointF>& points = geometryPoints();

    if (points.size() == 0) {
        deletePoints(m_items.childItems().count());
        return;
    }

    int diff = m_items.childItems().size() - points.size();

    if (diff > 0)
        deletePoints(diff);
    else if (diff < 0)
        createPoints(-diff);

    if (diff != 0)
        handleUpdated();

    QList<QGraphicsItem *> items = m_items.childItems();

    QRectF clipRect(QPointF(0,0),domain()->size());

    // Only zoom in if the clipRect fits inside int limits. QWidget::update() uses
    // a region that has to be compatible with QRect.
    if (clipRect.height() <= INT_MAX
            && clipRect.width() <= INT_MAX) {
        QVector<bool> offGridStatus = offGridStatusVector();
        const int seriesLastIndex = m_series->count() - 1;

        for (int i = 0; i < points.size(); i++) {
            QGraphicsItem *item = items.at(i);
            const QPointF &point = points.at(i);
            const QRectF &rect = item->boundingRect();
            // During remove animation series may have different number of points,
            // so ensure we don't go over the index. Animation handling itself ensures that
            // if there is actually no points in the series, then it won't generate a fake point,
            // so we can be assured there is always at least one point in m_series here.
            // Note that marker map values can be technically incorrect during the animation,
            // if it was caused by an insert, but this shouldn't be a problem as the points are
            // fake anyway. After remove animation stops, geometry is updated to correct one.
            m_markerMap[item] = m_series->at(qMin(seriesLastIndex, i));
            QPointF position;
            position.setX(point.x() - rect.width() / 2);
            position.setY(point.y() - rect.height() / 2);
            item->setPos(position);

            if (!m_visible || offGridStatus.at(i))
                item->setVisible(false);
            else
                item->setVisible(true);
        }

        prepareGeometryChange();
        m_rect = clipRect;
    }
}

void ScatterChartItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (m_series->useOpenGL())
        return;

    QRectF clipRect = QRectF(QPointF(0, 0), domain()->size());

    painter->save();
    painter->setClipRect(clipRect);

    if (m_pointLabelsVisible) {
        if (m_pointLabelsClipping)
            painter->setClipping(true);
        else
            painter->setClipping(false);
        m_series->d_func()->drawSeriesPointLabels(painter, m_points,
                                                  m_series->markerSize() / 2
                                                  + m_series->pen().width());
    }

    painter->restore();
}

void ScatterChartItem::setPen(const QPen &pen)
{
    foreach (QGraphicsItem *item , m_items.childItems())
        static_cast<QAbstractGraphicsShapeItem*>(item)->setPen(pen);
}

void ScatterChartItem::setBrush(const QBrush &brush)
{
    foreach (QGraphicsItem *item , m_items.childItems())
        static_cast<QAbstractGraphicsShapeItem*>(item)->setBrush(brush);
}

void ScatterChartItem::handleUpdated()
{
    if (m_series->useOpenGL()) {
        if ((m_series->isVisible() != m_visible)) {
            m_visible = m_series->isVisible();
            refreshGlChart();
        }
        return;
    }

    int count = m_items.childItems().count();
    if (count == 0)
        return;

    bool recreate = m_visible != m_series->isVisible()
                    || m_size != m_series->markerSize()
                    || m_shape != m_series->markerShape();
    m_visible = m_series->isVisible();
    m_size = m_series->markerSize();
    m_shape = m_series->markerShape();
    setVisible(m_visible);
    setOpacity(m_series->opacity());
    m_pointLabelsFormat = m_series->pointLabelsFormat();
    m_pointLabelsVisible = m_series->pointLabelsVisible();
    m_pointLabelsFont = m_series->pointLabelsFont();
    m_pointLabelsColor = m_series->pointLabelsColor();
    m_pointLabelsClipping = m_series->pointLabelsClipping();

    if (recreate) {
        deletePoints(count);
        createPoints(count);

        // Updating geometry is now safe, because it won't call handleUpdated unless it creates/deletes points
        updateGeometry();
    }

    setPen(m_series->pen());
    setBrush(m_series->brush());
    update();
}

#include "moc_scatterchartitem_p.cpp"

QT_CHARTS_END_NAMESPACE
