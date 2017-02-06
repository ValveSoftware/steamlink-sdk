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

#include <QtCharts/QScatterSeries>
#include <private/qscatterseries_p.h>
#include <private/scatterchartitem_p.h>
#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>
#include <private/scatteranimation_p.h>
#include <private/qchart_p.h>

/*!
    \class QScatterSeries
    \inmodule Qt Charts
    \brief The QScatterSeries class is used for making scatter charts.

    The scatter data is displayed as a collection of points on the chart. Each point determines the position on the horizontal axis
    and the vertical axis.

    \image examples_scatterchart.png

    Creating basic scatter chart is simple:
    \code
    QScatterSeries* series = new QScatterSeries();
    series->append(0, 6);
    series->append(2, 4);
    ...
    chart->addSeries(series);
    \endcode
*/
/*!
    \qmltype ScatterSeries
    \instantiates QScatterSeries
    \inqmlmodule QtCharts

    \inherits XYSeries

    \brief The ScatterSeries type is used for making scatter charts.

    The following QML shows how to create a chart with two simple scatter series:
    \snippet qmlchart/qml/qmlchart/View5.qml 1

    \beginfloatleft
    \image examples_qmlchart5.png
    \endfloat
    \clearfloat
*/

/*!
    \enum QScatterSeries::MarkerShape

    This enum describes the shape used when rendering marker items.

    \value MarkerShapeCircle
    \value MarkerShapeRectangle
*/

/*!
    \property QScatterSeries::brush
    Brush used to draw the series.
*/

/*!
    \property QScatterSeries::color
    Fill (brush) color of the series. This is a convenience property for modifying the color of brush.
    \sa QScatterSeries::brush()
*/

/*!
    \property QScatterSeries::borderColor
    Line (pen) color of the series. This is a convenience property for modifying the color of pen.
    \sa QScatterSeries::pen()
*/
/*!
    \qmlproperty color ScatterSeries::borderColor
    Border (pen) color of the series.
*/

/*!
    \qmlproperty real ScatterSeries::borderWidth
    The width of the border line. By default the width is 2.0.
*/

/*!
    \property QScatterSeries::markerShape
    Defines the shape of the marker used to draw the points in the series. The default shape is MarkerShapeCircle.
*/
/*!
    \qmlproperty MarkerShape ScatterSeries::markerShape
    Defines the shape of the marker used to draw the points in the series. One of ScatterSeries
    ScatterSeries.MarkerShapeCircle or ScatterSeries.MarkerShapeRectangle.
    The default shape is ScatterSeries.MarkerShapeCircle.
*/

/*!
    \property QScatterSeries::markerSize
    Defines the size of the marker used to draw the points in the series. The default size is 15.0.
*/
/*!
    \qmlproperty real ScatterSeries::markerSize
    Defines the size of the marker used to draw the points in the series. The default size is 15.0.
*/

/*!
    \qmlproperty QString ScatterSeries::brushFilename
    The name of the file used as a brush for the series.
*/

/*!
    \fn void QScatterSeries::colorChanged(QColor color)
    Signal is emitted when the fill (brush) color has changed to \a color.
*/
/*!
    \qmlsignal ScatterSeries::onColorChanged(color color)
    Signal is emitted when the fill (brush) color has changed to \a color.
*/

/*!
    \fn void QScatterSeries::borderColorChanged(QColor color)
    Signal is emitted when the line (pen) color has changed to \a color.
*/
/*!
    \qmlsignal ScatterSeries::onBorderColorChanged(color color)
    Signal is emitted when the line (pen) color has changed to \a color.
*/

/*!
    \fn QAbstractSeries::SeriesType QScatterSeries::type() const
    Returns QAbstractSeries::SeriesTypeScatter.
    \sa QAbstractSeries, SeriesType
*/

/*!
    \fn void QScatterSeries::markerShapeChanged(MarkerShape shape)
    Signal is emitted when the marker shape has changed to \a shape.
*/
/*!
    \qmlsignal ScatterSeries::onMarkerShapeChanged(MarkerShape shape)
    Signal is emitted when the marker shape has changed to \a shape.
*/
/*!
    \fn void QScatterSeries::markerSizeChanged(qreal size)
    Signal is emitted when the marker size has changed to \a size.
*/
/*!
    \qmlsignal ScatterSeries::onMarkerSizeChanged(real size)
    Signal is emitted when the marker size has changed to \a size.
*/

QT_CHARTS_BEGIN_NAMESPACE

/*!
    Constructs a series object which is a child of \a parent.
*/
QScatterSeries::QScatterSeries(QObject *parent)
    : QXYSeries(*new QScatterSeriesPrivate(this), parent)
{
}

/*!
    Destroys the object. Note that adding series to QChart transfers the ownership to the chart.
*/
QScatterSeries::~QScatterSeries()
{
    Q_D(QScatterSeries);
    if (d->m_chart)
        d->m_chart->removeSeries(this);
}

QAbstractSeries::SeriesType QScatterSeries::type() const
{
    return QAbstractSeries::SeriesTypeScatter;
}

/*!
    Sets \a pen used for drawing points' border on the chart. If the pen is not defined, the
    pen from chart theme is used.
    \sa QChart::setTheme()
*/
void QScatterSeries::setPen(const QPen &pen)
{
    Q_D(QXYSeries);
    if (d->m_pen != pen) {
        bool emitColorChanged = d->m_pen.color() != pen.color();
        d->m_pen = pen;
        emit d->updated();
        if (emitColorChanged)
            emit borderColorChanged(pen.color());
    }
}

/*!
    Sets \a brush used for drawing points on the chart. If the brush is not defined, brush
    from chart theme setting is used.
    \sa QChart::setTheme()
*/
void QScatterSeries::setBrush(const QBrush &brush)
{
    Q_D(QScatterSeries);
    if (d->m_brush != brush) {
        bool emitColorChanged = d->m_brush.color() != brush.color();
        d->m_brush = brush;
        emit d->updated();
        if (emitColorChanged)
            emit colorChanged(brush.color());
    }
}

QBrush QScatterSeries::brush() const
{
    Q_D(const QScatterSeries);
    if (d->m_brush == QChartPrivate::defaultBrush())
        return QBrush();
    else
        return d->m_brush;
}

void QScatterSeries::setColor(const QColor &color)
{
    QBrush b = brush();
    if (b == QChartPrivate::defaultBrush())
        b = QBrush();
    if (b == QBrush())
        b.setStyle(Qt::SolidPattern);
    b.setColor(color);
    setBrush(b);
}

QColor QScatterSeries::color() const
{
    return brush().color();
}

void QScatterSeries::setBorderColor(const QColor &color)
{
    QPen p = pen();
    if (p == QChartPrivate::defaultPen())
        p = QPen();
    p.setColor(color);
    setPen(p);
}

QColor QScatterSeries::borderColor() const
{
    return pen().color();
}

QScatterSeries::MarkerShape QScatterSeries::markerShape() const
{
    Q_D(const QScatterSeries);
    return d->m_shape;
}

void QScatterSeries::setMarkerShape(MarkerShape shape)
{
    Q_D(QScatterSeries);
    if (d->m_shape != shape) {
        d->m_shape = shape;
        emit d->updated();
        emit markerShapeChanged(shape);
    }
}

qreal QScatterSeries::markerSize() const
{
    Q_D(const QScatterSeries);
    return d->m_size;
}

void QScatterSeries::setMarkerSize(qreal size)
{
    Q_D(QScatterSeries);

    if (!qFuzzyCompare(d->m_size, size)) {
        d->m_size = size;
        emit d->updated();
        emit markerSizeChanged(size);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QScatterSeriesPrivate::QScatterSeriesPrivate(QScatterSeries *q)
    : QXYSeriesPrivate(q),
      m_shape(QScatterSeries::MarkerShapeCircle),
      m_size(15.0)
{
}

void QScatterSeriesPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_Q(QScatterSeries);
    ScatterChartItem *scatter = new ScatterChartItem(q,parent);
    m_item.reset(scatter);
    QAbstractSeriesPrivate::initializeGraphics(parent);
}

void QScatterSeriesPrivate::initializeTheme(int index, ChartTheme* theme, bool forced)
{
    Q_Q(QScatterSeries);
    const QList<QColor> colors = theme->seriesColors();
    const QList<QGradient> gradients = theme->seriesGradients();

    if (forced || QChartPrivate::defaultPen() == m_pen) {
        QPen pen;
        pen.setColor(ChartThemeManager::colorAt(gradients.at(index % gradients.size()), 0.0));
        pen.setWidthF(2);
        q->setPen(pen);
    }

    if (forced || QChartPrivate::defaultBrush() == m_brush) {
        QBrush brush(colors.at(index % colors.size()));
        q->setBrush(brush);
    }

    if (forced || QChartPrivate::defaultPen().color() == m_pointLabelsColor) {
        QColor color = theme->labelBrush().color();
        q->setPointLabelsColor(color);
    }
}

void QScatterSeriesPrivate::initializeAnimations(QChart::AnimationOptions options, int duration,
                                                 QEasingCurve &curve)
{
    ScatterChartItem *item = static_cast<ScatterChartItem *>(m_item.data());
    Q_ASSERT(item);

    if (item->animation())
        item->animation()->stopAndDestroyLater();

    if (options.testFlag(QChart::SeriesAnimations))
        item->setAnimation(new ScatterAnimation(item, duration, curve));
    else
        item->setAnimation(0);

    QAbstractSeriesPrivate::initializeAnimations(options, duration, curve);
}

#include "moc_qscatterseries.cpp"

QT_CHARTS_END_NAMESPACE
