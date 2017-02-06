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

#include <QtCharts/QLegend>
#include <private/qlegend_p.h>
#include <QtCharts/QAbstractSeries>
#include <private/qabstractseries_p.h>
#include <private/qchart_p.h>
#include <private/legendlayout_p.h>
#include <private/chartpresenter_p.h>
#include <private/abstractchartlayout_p.h>
#include <QtCharts/QLegendMarker>
#include <private/qlegendmarker_p.h>
#include <private/legendmarkeritem_p.h>
#include <private/chartdataset_p.h>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtWidgets/QGraphicsItemGroup>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QLegend
    \inmodule Qt Charts
    \inherits QGraphicsWidget
    \brief The QLegend class displays the legend of a chart.

    A legend is a graphical object that displays the legend of a chart. The legend state is updated
    by QChart when series change. By default, the legend is attached to the chart, but it can be
    detached to make it independent of chart layout. Legend objects cannot be created or deleted,
    but they can be referenced via the QChart class.

    \image examples_percentbarchart_legend.png

    \sa QChart
*/
/*!
    \qmltype Legend
    \instantiates QLegend
    \inqmlmodule QtCharts

    \brief Displays the legend of a chart.

    A legend is a graphical object that displays the legend of a chart. The legend state is updated
    by the ChartView type when series change. The \l Legend type properties can be attached to the
    ChartView type. For example:
    \code
        ChartView {
            legend.visible: true
            legend.alignment: Qt.AlignBottom
            // Add a few series...
        }
    \endcode

    \image examples_percentbarchart_legend.png

    \note There is no QML API available for modifying legend markers. Markers can be modified by
    creating a custom legend, as illustrated by \l {qmlcustomlegend}{Qml Custom Example}.
*/

/*!
    \property QLegend::alignment
    \brief How the legend is aligned with the chart.

    Can be Qt::AlignTop, Qt::AlignBottom, Qt::AlignLeft, Qt::AlignRight. If you set more than one
    flag, the result is undefined.
*/
/*!
    \qmlproperty alignment Legend::alignment
    Defines how the legend is aligned with the chart. Can be \l{Qt::AlignLeft}{Qt.AlignLeft},
    \l{Qt::AlignRight}{Qt.AlignRight}, \l{Qt::AlignBottom}{Qt.AlignBottom}, or
    \l{Qt::AlignTop}{Qt.AlignTop}. If you set more than one flag, the result is undefined.
*/

/*!
    \property QLegend::backgroundVisible
    \brief Whether the legend background is visible.
*/
/*!
    \qmlproperty bool Legend::backgroundVisible
    Whether the legend background is visible.
*/

/*!
    \qmlproperty bool Legend::visible
    Whether the legend is visible.

    By default, this property is \c true.
    \sa QGraphicsObject::visible
*/

/*!
    \property QLegend::color
    \brief The background (brush) color of the legend.

    If you change the color of the legend, the style of the legend brush is set to
    Qt::SolidPattern.
*/
/*!
    \qmlproperty color Legend::color
    The background (brush) color of the legend.
*/

/*!
    \property QLegend::borderColor
    \brief The line color of the legend.
*/
/*!
    \qmlproperty color Legend::borderColor
    The line color of the legend.
*/

/*!
    \property QLegend::font
    \brief The font of the markers used by the legend.
*/
/*!
    \qmlproperty Font Legend::font
    The font of the markers used by the legend.
*/

/*!
    \property QLegend::labelColor
    \brief The color of the brush used to draw labels.
*/
/*!
    \qmlproperty color Legend::labelColor
    The color of the brush used to draw labels.
*/

/*!
    \property QLegend::reverseMarkers
    \brief Whether reverse order is used for the markers in the legend.

    This property is \c false by default.
*/
/*!
    \qmlproperty bool Legend::reverseMarkers
    Whether reverse order is used for the markers in the legend. This property
    is \c false by default.
*/

/*!
    \property QLegend::showToolTips
    \brief Whether tooltips are shown when the text is truncated.

    This property is \c false by default.
*/

/*!
    \qmlproperty bool Legend::showToolTips
    Whether tooltips are shown when the text is truncated. This property is \c false by default.
    This property currently has no effect as there is no support for tooltips in QML.
*/

/*!
    \fn void QLegend::backgroundVisibleChanged(bool)
    This signal is emitted when the visibility of the legend background changes to \a visible.
*/

/*!
    \fn void QLegend::colorChanged(QColor)
    This signal is emitted when the color of the legend background changes to \a color.
*/

/*!
    \fn void QLegend::borderColorChanged(QColor)
    This signal is emitted when the border color of the legend background changes to \a color.
*/

/*!
    \fn void QLegend::fontChanged(QFont)
    This signal is emitted when the font of the markers of the legend changes to \a font.
*/

/*!
    \fn void QLegend::labelColorChanged(QColor color)
    This signal is emitted when the color of the brush used to draw the legend
    labels changes to \a color.
*/

/*!
    \fn void QLegend::reverseMarkersChanged(bool)
    This signal is emitted when the use of reverse order for the markers in the
    legend is changed to \a reverseMarkers.
*/

/*!
    \fn void QLegend::showToolTipsChanged(bool showToolTips)
    This signal is emitted when the visibility of tooltips is changed to \a showToolTips.
*/

QLegend::QLegend(QChart *chart): QGraphicsWidget(chart),
    d_ptr(new QLegendPrivate(chart->d_ptr->m_presenter, chart, this))
{
    setZValue(ChartPresenter::LegendZValue);
    setFlags(QGraphicsItem::ItemClipsChildrenToShape);
    QObject::connect(chart->d_ptr->m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)), d_ptr.data(), SLOT(handleSeriesAdded(QAbstractSeries*)));
    QObject::connect(chart->d_ptr->m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)), d_ptr.data(), SLOT(handleSeriesRemoved(QAbstractSeries*)));
    setLayout(d_ptr->m_layout);
}

/*!
    Destroys the legend object. The legend is always owned by a QChart, so an application
    should never call this function.
*/
QLegend::~QLegend()
{
}

/*!
 \internal
 */
void QLegend::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!d_ptr->m_backgroundVisible)
        return;

    painter->setOpacity(opacity());
    painter->setPen(d_ptr->m_pen);
    painter->setBrush(d_ptr->m_brush);
    painter->drawRoundRect(rect(), d_ptr->roundness(rect().width()), d_ptr->roundness(rect().height()));
}


/*!
    Sets the \a brush that is used to draw the background of the legend.
 */
void QLegend::setBrush(const QBrush &brush)
{
    if (d_ptr->m_brush != brush) {
        d_ptr->m_brush = brush;
        update();
        emit colorChanged(brush.color());
    }
}

/*!
    Returns the brush used by the legend.
 */
QBrush QLegend::brush() const
{
    if (d_ptr->m_brush == QChartPrivate::defaultBrush())
        return QBrush();
    else
        return d_ptr->m_brush;
}

void QLegend::setColor(QColor color)
{
    QBrush b = brush();
    if (b.style() != Qt::SolidPattern || b.color() != color) {
        b.setStyle(Qt::SolidPattern);
        b.setColor(color);
        setBrush(b);
    }
}

QColor QLegend::color()
{
    return d_ptr->m_brush.color();
}

/*!
    Sets the \a pen that is used to draw the legend borders.
 */
void QLegend::setPen(const QPen &pen)
{
    if (d_ptr->m_pen != pen) {
        d_ptr->m_pen = pen;
        update();
        emit borderColorChanged(pen.color());
    }
}

/*!
    Returns the pen used by the legend.
 */

QPen QLegend::pen() const
{
    if (d_ptr->m_pen == QChartPrivate::defaultPen())
        return QPen();
    else
        return d_ptr->m_pen;
}

void QLegend::setFont(const QFont &font)
{
    if (d_ptr->m_font != font) {
        // Hide items to avoid flickering
        d_ptr->items()->setVisible(false);
        d_ptr->m_font = font;
        foreach (QLegendMarker *marker, d_ptr->markers()) {
            marker->setFont(d_ptr->m_font);
        }
        layout()->invalidate();
        emit fontChanged(font);
    }
}

QFont QLegend::font() const
{
    return d_ptr->m_font;
}

void QLegend::setBorderColor(QColor color)
{
    QPen p = pen();
    if (p.color() != color) {
        p.setColor(color);
        setPen(p);
    }
}

QColor QLegend::borderColor()
{
    return d_ptr->m_pen.color();
}

/*!
    Sets the brush used to draw the legend labels to \a brush.
*/
void QLegend::setLabelBrush(const QBrush &brush)
{
    if (d_ptr->m_labelBrush != brush) {
        d_ptr->m_labelBrush = brush;
        foreach (QLegendMarker *marker, d_ptr->markers()) {
            marker->setLabelBrush(d_ptr->m_labelBrush);
            // Note: The pen of the marker rectangle could be exposed in the public QLegend API
            // instead of mapping it from label brush color
            marker->setPen(brush.color());
        }
        emit labelColorChanged(brush.color());
    }
}

/*!
    Returns the brush used to draw labels.
*/
QBrush QLegend::labelBrush() const
{
    if (d_ptr->m_labelBrush == QChartPrivate::defaultBrush())
        return QBrush();
    else
        return d_ptr->m_labelBrush;
}

void QLegend::setLabelColor(QColor color)
{
    QBrush b = labelBrush();
    if (b.style() != Qt::SolidPattern || b.color() != color) {
        b.setStyle(Qt::SolidPattern);
        b.setColor(color);
        setLabelBrush(b);
    }
}

QColor QLegend::labelColor() const
{
    return d_ptr->m_labelBrush.color();
}


void QLegend::setAlignment(Qt::Alignment alignment)
{
    if (d_ptr->m_alignment != alignment) {
        d_ptr->m_alignment = alignment;
        layout()->invalidate();
    }
}

Qt::Alignment QLegend::alignment() const
{
    return d_ptr->m_alignment;
}

/*!
    Detaches the legend from the chart. The chart will no longer adjust the layout of the legend.
 */
void QLegend::detachFromChart()
{
    d_ptr->m_attachedToChart = false;
//    layout()->invalidate();
    d_ptr->m_chart->layout()->invalidate();
    setParent(0);

}

/*!
    Attaches the legend to a chart. The chart may adjust the layout of the legend.
 */
void QLegend::attachToChart()
{
    d_ptr->m_attachedToChart = true;
//    layout()->invalidate();
    d_ptr->m_chart->layout()->invalidate();
    setParent(d_ptr->m_chart);
}

/*!
    Returns \c true, if the legend is attached to a chart.
 */
bool QLegend::isAttachedToChart()
{
    return d_ptr->m_attachedToChart;
}

/*!
    Sets the visibility of the legend background to \a visible.
 */
void QLegend::setBackgroundVisible(bool visible)
{
    if (d_ptr->m_backgroundVisible != visible) {
        d_ptr->m_backgroundVisible = visible;
        update();
        emit backgroundVisibleChanged(visible);
    }
}

/*!
    Returns the visibility of the legend background.
 */
bool QLegend::isBackgroundVisible() const
{
    return d_ptr->m_backgroundVisible;
}

/*!
    Returns the list of markers in the legend. The list can be filtered by specifying
    the \a series for which the markers are returned.
*/
QList<QLegendMarker*> QLegend::markers(QAbstractSeries *series) const
{
    return d_ptr->markers(series);
}

bool QLegend::reverseMarkers()
{
    return d_ptr->m_reverseMarkers;
}

void QLegend::setReverseMarkers(bool reverseMarkers)
{
    if (d_ptr->m_reverseMarkers != reverseMarkers) {
        d_ptr->m_reverseMarkers = reverseMarkers;
        layout()->invalidate();
        emit reverseMarkersChanged(reverseMarkers);
    }
}

/*!
    Returns whether the tooltips are shown for the legend labels
    when they are elided.
*/

bool QLegend::showToolTips() const
{
    return d_ptr->m_showToolTips;
}

/*!
    When \a show is \c true, the legend labels will show a tooltip when
    the mouse hovers over them if the label itself is shown elided.
    This is \c false by default.
*/

void QLegend::setShowToolTips(bool show)
{
    if (d_ptr->m_showToolTips != show) {
        d_ptr->m_showToolTips = show;
        d_ptr->updateToolTips();
        emit showToolTipsChanged(show);
    }
}

/*!
    \internal
    \a event, see QGraphicsWidget for details.
 */
void QLegend::hideEvent(QHideEvent *event)
{
    if (isAttachedToChart())
        d_ptr->m_presenter->layout()->invalidate();
    QGraphicsWidget::hideEvent(event);
}
/*!
    \internal
    \a event, see QGraphicsWidget for details.
 */
void QLegend::showEvent(QShowEvent *event)
{
    if (isAttachedToChart())
        layout()->invalidate();
    QGraphicsWidget::showEvent(event);
    //layout activation will show the items
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QLegendPrivate::QLegendPrivate(ChartPresenter *presenter, QChart *chart, QLegend *q)
    : q_ptr(q),
      m_presenter(presenter),
      m_layout(new LegendLayout(q)),
      m_chart(chart),
      m_items(new QGraphicsItemGroup(q)),
      m_alignment(Qt::AlignTop),
      m_brush(QChartPrivate::defaultBrush()),
      m_pen(QChartPrivate::defaultPen()),
      m_labelBrush(QChartPrivate::defaultBrush()),
      m_diameter(5),
      m_attachedToChart(true),
      m_backgroundVisible(false),
      m_reverseMarkers(false),
      m_showToolTips(false)
{
    m_items->setHandlesChildEvents(false);
}

QLegendPrivate::~QLegendPrivate()
{

}

void QLegendPrivate::setOffset(const QPointF &offset)
{
    m_layout->setOffset(offset.x(), offset.y());
}

QPointF QLegendPrivate::offset() const
{
    return m_layout->offset();
}

int QLegendPrivate::roundness(qreal size)
{
    return 100 * m_diameter / int(size);
}

QList<QLegendMarker*> QLegendPrivate::markers(QAbstractSeries *series)
{
    // Return all markers
    if (!series) {
        return m_markers;
    }

    // Create filtered list
    QList<QLegendMarker *> markers;
    foreach (QLegendMarker *marker, m_markers) {
        if (marker->series() == series) {
            markers.append(marker);
        }
    }
    return markers;
}

void QLegendPrivate::handleSeriesAdded(QAbstractSeries *series)
{
    if (m_series.contains(series)) {
        return;
    }

    QList<QLegendMarker*> newMarkers = series->d_ptr->createLegendMarkers(q_ptr);
    decorateMarkers(newMarkers);
    addMarkers(newMarkers);

    QObject::connect(series->d_ptr.data(), SIGNAL(countChanged()), this, SLOT(handleCountChanged()));
    QObject::connect(series, SIGNAL(visibleChanged()), this, SLOT(handleSeriesVisibleChanged()));

    m_series.append(series);
    m_items->setVisible(false);
    m_layout->invalidate();
}

void QLegendPrivate::handleSeriesRemoved(QAbstractSeries *series)
{
    if (m_series.contains(series)) {
        m_series.removeOne(series);
    }

    // Find out, which markers to remove
    QList<QLegendMarker *> removed;
    foreach (QLegendMarker *m, m_markers) {
        if (m->series() == series) {
            removed << m;
        }
    }
    removeMarkers(removed);

    QObject::disconnect(series->d_ptr.data(), SIGNAL(countChanged()), this, SLOT(handleCountChanged()));
    QObject::disconnect(series, SIGNAL(visibleChanged()), this, SLOT(handleSeriesVisibleChanged()));

    m_layout->invalidate();
}

void QLegendPrivate::handleSeriesVisibleChanged()
{
    QAbstractSeries *series = qobject_cast<QAbstractSeries *> (sender());
    Q_ASSERT(series);

    foreach (QLegendMarker *marker, m_markers) {
        if (marker->series() == series) {
            marker->setVisible(series->isVisible());
        }
    }

    if (m_chart->isVisible())
        m_layout->invalidate();
}

void QLegendPrivate::handleCountChanged()
{
    // Here we handle the changes in marker count.
    // Can happen for example when pieslice(s) have been added to or removed from pieseries.

    QAbstractSeriesPrivate *series = qobject_cast<QAbstractSeriesPrivate *> (sender());
    QList<QLegendMarker *> createdMarkers = series->createLegendMarkers(q_ptr);

    // Find out removed markers and created markers
    QList<QLegendMarker *> removedMarkers;
    foreach (QLegendMarker *oldMarker, m_markers) {
        // we have marker, which is related to sender.
        if (oldMarker->series() == series->q_ptr) {
            bool found = false;
            foreach(QLegendMarker *newMarker, createdMarkers) {
                // New marker considered existing if:
                // - d_ptr->relatedObject() is same for both markers.
                if (newMarker->d_ptr->relatedObject() == oldMarker->d_ptr->relatedObject()) {
                    // Delete the new marker, since we already have existing marker, that might be connected on user side.
                    found = true;
                    createdMarkers.removeOne(newMarker);
                    delete newMarker;
                }
            }
            if (!found) {
                // No related object found for marker, add to removedMarkers list
                removedMarkers << oldMarker;
            }
        }
    }

    removeMarkers(removedMarkers);
    decorateMarkers(createdMarkers);
    addMarkers(createdMarkers);

    q_ptr->layout()->invalidate();
}

void QLegendPrivate::addMarkers(QList<QLegendMarker *> markers)
{
    foreach (QLegendMarker *marker, markers) {
        m_items->addToGroup(marker->d_ptr.data()->item());
        m_markers << marker;
        m_markerHash.insert(marker->d_ptr->item(), marker);
    }
}

void QLegendPrivate::removeMarkers(QList<QLegendMarker *> markers)
{
    foreach (QLegendMarker *marker, markers) {
        marker->d_ptr->item()->setVisible(false);
        m_items->removeFromGroup(marker->d_ptr->item());
        m_markers.removeOne(marker);
        m_markerHash.remove(marker->d_ptr->item());
        delete marker;
    }
}

void QLegendPrivate::decorateMarkers(QList<QLegendMarker *> markers)
{
    foreach (QLegendMarker *marker, markers) {
        marker->setFont(m_font);
        marker->setLabelBrush(m_labelBrush);
    }
}

void QLegendPrivate::updateToolTips()
{
    foreach (QLegendMarker *m, m_markers) {
        if (m->d_ptr->m_item->displayedLabel() != m->label())
            m->d_ptr->m_item->setToolTip(m->label());
        else
            m->d_ptr->m_item->setToolTip(QString());
    }
}
#include "moc_qlegend.cpp"
#include "moc_qlegend_p.cpp"

QT_CHARTS_END_NAMESPACE
