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

#include <QtCharts/QXYSeries>
#include <private/qxyseries_p.h>
#include <private/abstractdomain_p.h>
#include <QtCharts/QValueAxis>
#include <private/xychart_p.h>
#include <QtCharts/QXYLegendMarker>
#include <private/charthelpers_p.h>
#include <private/qchart_p.h>
#include <QtGui/QPainter>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QXYSeries
    \inmodule Qt Charts
    \brief The QXYSeries class is a base class for line, spline and scatter series.
*/
/*!
    \qmltype XYSeries
    \instantiates QXYSeries
    \inqmlmodule QtCharts

    \inherits AbstractSeries

    \brief The XYSeries type is a base type for line, spline and scatter series.

    The XYSeries class is a base class for line, spline and scatter series.
    The class cannot be instantiated directly.
*/

/*!
    \qmlproperty AbstractAxis XYSeries::axisX
    The x axis used for the series. If you leave both axisX and axisXTop undefined, a ValueAxis is created for
    the series.
    \sa axisXTop
*/

/*!
    \qmlproperty AbstractAxis XYSeries::axisY
    The y axis used for the series. If you leave both axisY and axisYRight undefined, a ValueAxis is created for
    the series.
    \sa axisYRight
*/

/*!
    \qmlproperty AbstractAxis XYSeries::axisXTop
    The x axis used for the series, drawn on top of the chart view. Note that you can only provide either axisX or
    axisXTop, but not both.
    \sa axisX
*/

/*!
    \qmlproperty AbstractAxis XYSeries::axisYRight
    The y axis used for the series, drawn to the right on the chart view. Note that you can only provide either axisY
    or axisYRight, but not both.
    \sa axisY
*/

/*!
    \qmlproperty AbstractAxis XYSeries::axisAngular
    The angular axis used for the series, drawn around the polar chart view.
    \sa axisX
*/

/*!
    \qmlproperty AbstractAxis XYSeries::axisRadial
    The radial axis used for the series, drawn inside the polar chart view.
    \sa axisY
*/

/*!
    \property QXYSeries::pointsVisible
    Controls if the data points are visible and should be drawn.
*/
/*!
    \qmlproperty bool XYSeries::pointsVisible
    Controls if the data points are visible and should be drawn.
*/

/*!
   \fn QPen QXYSeries::pen() const
   \brief  Returns pen used to draw points for series.
    \sa setPen()
*/

/*!
   \fn QBrush QXYSeries::brush() const
   \brief  Returns brush used to draw points for series.
    \sa setBrush()
*/

/*!
    \property QXYSeries::color
    The color of the series. This is line (pen) color in case of QLineSeries or QSplineSeries and
    fill (brush) color in case of QScatterSeries or QAreaSeries.
    \sa pen(), brush()
*/
/*!
    \qmlproperty color XYSeries::color
    The color of the series. This is line (pen) color in case of LineSeries or SplineSeries and
    fill (brush) color in case of ScatterSeries or AreaSeries.
*/

/*!
    \property QXYSeries::pointLabelsFormat
    The \a format used for showing labels with series points.

    QXYSeries supports the following format tags:
    \table
      \row
        \li @xPoint      \li The x value of the data point
      \row
        \li @yPoint      \li The y value of the data point
    \endtable

    For example, the following usage of the format tags would produce labels that have the data
    point (x, y) shown inside brackets separated by a comma:
    \code
    series->setPointLabelsFormat("(@xPoint, @yPoint)");
    \endcode

    By default, the labels format is set to '@xPoint, @yPoint'. The labels are shown on the plot
    area, labels on the edge of the plot area are cut. If the points are close to each other the
    labels may overlap.

    \sa pointLabelsVisible, pointLabelsFont, pointLabelsColor
*/
/*!
    \qmlproperty string XYSeries::pointLabelsFormat
    The \a format used for showing labels with series points.

    \sa pointLabelsFormat, pointLabelsVisible, pointLabelsFont, pointLabelsColor
*/
/*!
    \fn void QXYSeries::pointLabelsFormatChanged(const QString &format)
    Signal is emitted when the \a format of data point labels is changed.
*/
/*!
    \qmlsignal XYSeries::onPointLabelsFormatChanged(string format)
    Signal is emitted when the \a format of data point labels is changed.
*/

/*!
    \property QXYSeries::pointLabelsVisible
    Defines the visibility for data point labels. False by default.

    \sa pointLabelsFormat, pointLabelsClipping
*/
/*!
    \qmlproperty bool XYSeries::pointLabelsVisible
    Defines the visibility for data point labels.

    \sa pointLabelsFormat, pointLabelsClipping
*/
/*!
    \fn void QXYSeries::pointLabelsVisibilityChanged(bool visible)
    The visibility of the data point labels is changed to \a visible.
*/
/*!
    \qmlsignal XYSeries::onPointLabelsVisibilityChanged(bool visible)
    The visibility of the data point labels is changed to \a visible.
*/

/*!
    \property QXYSeries::pointLabelsFont
    Defines the font used for data point labels.

    \sa pointLabelsFormat
*/
/*!
    \qmlproperty font XYSeries::pointLabelsFont
    Defines the font used for data point labels.

    \sa pointLabelsFormat
*/
/*!
    \fn void QXYSeries::pointLabelsFontChanged(const QFont &font);
    The font used for data point labels is changed to \a font.
*/
/*!
    \qmlsignal XYSeries::onPointLabelsFontChanged(Font font)
    The font used for data point labels is changed to \a font.
*/

/*!
    \property QXYSeries::pointLabelsColor
    Defines the color used for data point labels. By default, the color is the color of the brush
    defined in theme for labels.

    \sa pointLabelsFormat
*/
/*!
    \qmlproperty font XYSeries::pointLabelsColor
    Defines the color used for data point labels. By default, the color is the color of the brush
    defined in theme for labels.

    \sa pointLabelsFormat
*/
/*!
    \fn void QXYSeries::pointLabelsColorChanged(const QColor &color);
    The color used for data point labels is changed to \a color.
*/
/*!
    \qmlsignal XYSeries::onPointLabelsColorChanged(Color color)
    The color used for data point labels is changed to \a color.
*/

/*!
    \property QXYSeries::pointLabelsClipping
    Defines the clipping for data point labels. True by default. The labels on the edge of the plot
    area are cut when clipping is enabled.

    \sa pointLabelsVisible
*/
/*!
    \qmlproperty bool XYSeries::pointLabelsClipping
    Defines the clipping for data point labels. True by default. The labels on the edge of the plot
    area are cut when clipping is enabled.

    \sa pointLabelsVisible
*/
/*!
    \fn void QXYSeries::pointLabelsClippingChanged(bool clipping)
    The clipping of the data point labels is changed to \a clipping.
*/
/*!
    \qmlsignal XYSeries::onPointLabelsClippingChanged(bool clipping)
    The clipping of the data point labels is changed to \a clipping.
*/

/*!
    \fn void QXYSeries::clicked(const QPointF& point)
    \brief Signal is emitted when user clicks the \a point on chart. The \a point is the point
    where the press was triggered.
    \sa pressed, released, doubleClicked
*/
/*!
    \qmlsignal XYSeries::onClicked(QPointF point)
    Signal is emitted when user clicks the \a point on chart. The \a point is the point where the
    press was triggered. For example:
    \code
    LineSeries {
        XYPoint { x: 0; y: 0 }
        XYPoint { x: 1.1; y: 2.1 }
        onClicked: console.log("onClicked: " + point.x + ", " + point.y);
    }
    \endcode
    \sa onPressed, onReleased, onDoubleClicked
*/

/*!
    \fn void QXYSeries::hovered(const QPointF &point, bool state)
    This signal is emitted when user has hovered over or away from the series. \a point shows the origin (coordinate)
    of the hover event. \a state is true when user has hovered over the series and false when hover has moved away from
    the series.
*/
/*!
    \qmlsignal XYSeries::onHovered(point point, bool state)
    This signal is emitted when user has hovered over or away from the series. \a point shows the origin (coordinate)
    of the hover event. \a state is true when user has hovered over the series and false when hover has moved away from
    the series.
*/

/*!
    \fn void QXYSeries::pressed(const QPointF& point)
    \brief Signal is emitted when user presses the \a point on chart.
    \sa clicked, released, doubleClicked
*/
/*!
    \qmlsignal XYSeries::onPressed(QPointF point)
    Signal is emitted when user presses the \a point on chart. For example:
    \code
    LineSeries {
        XYPoint { x: 0; y: 0 }
        XYPoint { x: 1.1; y: 2.1 }
        onPressed: console.log("onPressed: " + point.x + ", " + point.y);
    }
    \endcode
    \sa onClicked, onReleased, onDoubleClicked
*/

/*!
    \fn void QXYSeries::released(const QPointF& point)
    \brief Signal is emitted when user releases a press that was triggered on a \a point on chart.
    \sa pressed, clicked, doubleClicked
*/
/*!
    \qmlsignal XYSeries::onReleased(QPointF point)
    Signal is emitted when user releases a press that was triggered on a \a point on chart.
    For example:
    \code
    LineSeries {
        XYPoint { x: 0; y: 0 }
        XYPoint { x: 1.1; y: 2.1 }
        onReleased: console.log("onReleased: " + point.x + ", " + point.y);
    }
    \endcode
    \sa onPressed, onClicked, onDoubleClicked
*/

/*!
    \fn void QXYSeries::doubleClicked(const QPointF& point)
    \brief Signal is emitted when user doubleclicks the \a point on chart. The \a point is the
    point where the first press was triggered.
    \sa pressed, released, clicked
*/
/*!
    \qmlsignal XYSeries::onDoubleClicked(QPointF point)
    Signal is emitted when user doubleclicks the \a point on chart. The \a point is the point where
    the first press was triggered. For example:
    \code
    LineSeries {
        XYPoint { x: 0; y: 0 }
        XYPoint { x: 1.1; y: 2.1 }
        onDoubleClicked: console.log("onDoubleClicked: " + point.x + ", " + point.y);
    }
    \endcode
    \sa onPressed, onReleased, onClicked
*/

/*!
    \fn void QXYSeries::pointReplaced(int index)
    Signal is emitted when a point has been replaced at \a index.
    \sa replace()
*/
/*!
    \qmlsignal XYSeries::onPointReplaced(int index)
    Signal is emitted when a point has been replaced at \a index.
*/

/*!
    \fn void QXYSeries::pointsReplaced()
    Signal is emitted when all points have been replaced with other points.
    \sa replace()
*/
/*!
    \qmlsignal XYSeries::onPointsReplaced()
    Signal is emitted when all points have been replaced with other points.
*/

/*!
    \fn void QXYSeries::pointAdded(int index)
    Signal is emitted when a point has been added at \a index.
    \sa append(), insert()
*/
/*!
    \qmlsignal XYSeries::onPointAdded(int index)
    Signal is emitted when a point has been added at \a index.
*/

/*!
    \fn void QXYSeries::pointRemoved(int index)
    Signal is emitted when a point has been removed from \a index.
    \sa remove()
*/

/*!
    \qmlsignal XYSeries::onPointRemoved(int index)
    Signal is emitted when a point has been removed from \a index.
*/

/*!
    \fn void QXYSeries::pointsRemoved(int index, int count)
    Signal is emitted when a \a count of points has been removed starting at \a index.
    \sa removePoints(), clear()
*/

/*!
    \qmlsignal XYSeries::onPointsRemoved(int index, int count)
    Signal is emitted when a \a count of points has been removed starting at \a index.
*/

/*!
    \fn void QXYSeries::colorChanged(QColor color)
    \brief Signal is emitted when the line (pen) color has changed to \a color.
*/
/*!
    \qmlsignal XYSeries::onColorChanged(color color)
    Signal is emitted when the line (pen) color has changed to \a color.
*/

/*!
    \fn void QXYSeries::penChanged(const QPen &pen)
    \brief Signal is emitted when the line pen has changed to \a pen.
*/

/*!
    \fn void QXYSeriesPrivate::updated()
    \brief \internal
*/

/*!
    \qmlmethod XYSeries::append(real x, real y)
    Append point (\a x, \a y) to the series
*/

/*!
    \qmlmethod XYSeries::replace(real oldX, real oldY, real newX, real newY)
    Replaces point (\a oldX, \a oldY) with point (\a newX, \a newY). Does nothing, if point (oldX, oldY) does not
    exist.
*/

/*!
    \qmlmethod XYSeries::remove(real x, real y)
    Removes point (\a x, \a y) from the series. Does nothing, if point (x, y) does not exist.
*/

/*!
    \qmlmethod XYSeries::remove(int index)
    Removes a point from the series at \a index.
*/

/*!
    \qmlmethod XYSeries::removePoints(int index, int count)
    Removes \a count points from the series starting at \a index.
*/

/*!
    \qmlmethod XYSeries::insert(int index, real x, real y)
    Inserts point (\a x, \a y) to the \a index. If index is 0 or smaller than 0 the point is prepended to the list of
    points. If index is the same as or bigger than count, the point is appended to the list of points.
*/

/*!
    \qmlmethod QPointF XYSeries::at(int index)
    Returns point at \a index. Returns (0, 0) if the index is not valid.
*/

/*!
    \internal

    Constructs empty series object which is a child of \a parent.
    When series object is added to QChart instance ownerships is transferred.
*/
QXYSeries::QXYSeries(QXYSeriesPrivate &d, QObject *parent)
    : QAbstractSeries(d, parent)
{
}

/*!
    Destroys the object. Series added to QChart instances are owned by those,
    and are destroyed when QChart instances are destroyed.
*/
QXYSeries::~QXYSeries()
{
}

/*!
    Adds data point (\a x, \a y) to the series.
 */
void QXYSeries::append(qreal x, qreal y)
{
    append(QPointF(x, y));
}

/*!
   This is an overloaded function.
   Adds data \a point to the series.
 */
void QXYSeries::append(const QPointF &point)
{
    Q_D(QXYSeries);

    if (isValidValue(point)) {
        d->m_points << point;
        emit pointAdded(d->m_points.count() - 1);
    }
}

/*!
   This is an overloaded function.
   Adds list of data \a points to the series.
 */
void QXYSeries::append(const QList<QPointF> &points)
{
    foreach (const QPointF &point , points)
        append(point);
}

/*!
  Replaces data point (\a oldX, \a oldY) with data point (\a newX, \a newY).
  \sa pointReplaced()
*/
void QXYSeries::replace(qreal oldX, qreal oldY, qreal newX, qreal newY)
{
    replace(QPointF(oldX, oldY), QPointF(newX, newY));
}

/*!
  Replaces \a oldPoint with \a newPoint.
  \sa pointReplaced()
*/
void QXYSeries::replace(const QPointF &oldPoint, const QPointF &newPoint)
{
    Q_D(QXYSeries);
    int index = d->m_points.indexOf(oldPoint);
    if (index == -1)
        return;
    replace(index, newPoint);
}

/*!
  Replaces the point at \a index with data point (\a newX, \a newY).
  \sa pointReplaced()
*/
void QXYSeries::replace(int index, qreal newX, qreal newY)
{
    replace(index, QPointF(newX, newY));
}

/*!
  Replaces the point at \a index with \a newPoint.
  \sa pointReplaced()
*/
void QXYSeries::replace(int index, const QPointF &newPoint)
{
    Q_D(QXYSeries);
    if (isValidValue(newPoint)) {
        d->m_points[index] = newPoint;
        emit pointReplaced(index);
    }
}

/*!
  Replaces the current points with \a points.
  \note This is much faster than replacing data points one by one,
  or first clearing all data, and then appending the new data. Emits QXYSeries::pointsReplaced()
  when the points have been replaced. However, note that using the overload that takes
  \c{QVector<QPointF>} as parameter is faster than using this overload.
  \sa pointsReplaced()
*/
void QXYSeries::replace(QList<QPointF> points)
{
    replace(points.toVector());
}

/*!
  Replaces the current points with \a points.
  \note This is much faster than replacing data points one by one,
  or first clearing all data, and then appending the new data. Emits QXYSeries::pointsReplaced()
  when the points have been replaced.
  \sa pointsReplaced()
*/
void QXYSeries::replace(QVector<QPointF> points)
{
    Q_D(QXYSeries);
    d->m_points = points;
    emit pointsReplaced();
}

/*!
  Removes the point (\a x, \a y) from the series.
  \sa pointRemoved()
*/
void QXYSeries::remove(qreal x, qreal y)
{
    remove(QPointF(x, y));
}

/*!
  Removes the \a point from the series.
  \sa pointRemoved()
*/
void QXYSeries::remove(const QPointF &point)
{
    Q_D(QXYSeries);
    int index = d->m_points.indexOf(point);
    if (index == -1)
        return;
    remove(index);
}

/*!
  Removes the point at \a index from the series.
  \sa pointRemoved()
*/
void QXYSeries::remove(int index)
{
    Q_D(QXYSeries);
    d->m_points.remove(index);
    emit pointRemoved(index);
}

/*!
  Removes \a count number of points from the series starting at \a index.
  \sa pointsRemoved()
*/
void QXYSeries::removePoints(int index, int count)
{
    // This function doesn't overload remove as there is chance for it to get mixed up with
    // remove(qreal, qreal) overload in some implicit casting cases.
    Q_D(QXYSeries);
    if (count > 0) {
        d->m_points.remove(index, count);
        emit pointsRemoved(index, count);
    }
}

/*!
   Inserts a \a point in the series at \a index position.
  \sa pointAdded()
*/
void QXYSeries::insert(int index, const QPointF &point)
{
    Q_D(QXYSeries);
    if (isValidValue(point)) {
        index = qMax(0, qMin(index, d->m_points.size()));
        d->m_points.insert(index, point);
        emit pointAdded(index);
    }
}

/*!
  Removes all points from the series.
  \sa pointsRemoved()
*/
void QXYSeries::clear()
{
    Q_D(QXYSeries);
    removePoints(0, d->m_points.size());
}

/*!
    Returns the points in the series as a list.
    Use QXYSeries::pointsVector() for better performance.
*/
QList<QPointF> QXYSeries::points() const
{
    Q_D(const QXYSeries);
    return d->m_points.toList();
}

/*!
    Returns the points in the series as a vector.
    This is more efficient that calling QXYSeries::points();
*/
QVector<QPointF> QXYSeries::pointsVector() const
{
    Q_D(const QXYSeries);
    return d->m_points;
}

/*!
    Returns point at \a index in internal points vector.
*/
const QPointF &QXYSeries::at(int index) const
{
    Q_D(const QXYSeries);
    return d->m_points.at(index);
}

/*!
    Returns number of data points within series.
*/
int QXYSeries::count() const
{
    Q_D(const QXYSeries);
    return d->m_points.count();
}


/*!
    Sets \a pen used for drawing points on the chart. If the pen is not defined, the
    pen from chart theme is used.
    \sa QChart::setTheme()
*/
void QXYSeries::setPen(const QPen &pen)
{
    Q_D(QXYSeries);
    if (d->m_pen != pen) {
        bool emitColorChanged = d->m_pen.color() != pen.color();
        d->m_pen = pen;
        emit d->updated();
        if (emitColorChanged)
            emit colorChanged(pen.color());
        emit penChanged(pen);
    }
}

QPen QXYSeries::pen() const
{
    Q_D(const QXYSeries);
    if (d->m_pen == QChartPrivate::defaultPen())
        return QPen();
    else
        return d->m_pen;
}

/*!
    Sets \a brush used for drawing points on the chart. If the brush is not defined, brush
    from chart theme setting is used.
    \sa QChart::setTheme()
*/
void QXYSeries::setBrush(const QBrush &brush)
{
    Q_D(QXYSeries);
    if (d->m_brush != brush) {
        d->m_brush = brush;
        emit d->updated();
    }
}

QBrush QXYSeries::brush() const
{
    Q_D(const QXYSeries);
    if (d->m_brush == QChartPrivate::defaultBrush())
        return QBrush();
    else
        return d->m_brush;
}

void QXYSeries::setColor(const QColor &color)
{
    QPen p = pen();
    if (p.color() != color) {
        p.setColor(color);
        setPen(p);
    }
}

QColor QXYSeries::color() const
{
    return pen().color();
}

void QXYSeries::setPointsVisible(bool visible)
{
    Q_D(QXYSeries);
    if (d->m_pointsVisible != visible) {
        d->m_pointsVisible = visible;
        emit d->updated();
    }
}

bool QXYSeries::pointsVisible() const
{
    Q_D(const QXYSeries);
    return d->m_pointsVisible;
}

void QXYSeries::setPointLabelsFormat(const QString &format)
{
    Q_D(QXYSeries);
    if (d->m_pointLabelsFormat != format) {
        d->m_pointLabelsFormat = format;
        emit pointLabelsFormatChanged(format);
    }
}

QString QXYSeries::pointLabelsFormat() const
{
    Q_D(const QXYSeries);
    return d->m_pointLabelsFormat;
}

void QXYSeries::setPointLabelsVisible(bool visible)
{
    Q_D(QXYSeries);
    if (d->m_pointLabelsVisible != visible) {
        d->m_pointLabelsVisible = visible;
        emit pointLabelsVisibilityChanged(visible);
    }
}

bool QXYSeries::pointLabelsVisible() const
{
    Q_D(const QXYSeries);
    return d->m_pointLabelsVisible;
}

void QXYSeries::setPointLabelsFont(const QFont &font)
{
    Q_D(QXYSeries);
    if (d->m_pointLabelsFont != font) {
        d->m_pointLabelsFont = font;
        emit pointLabelsFontChanged(font);
    }
}

QFont QXYSeries::pointLabelsFont() const
{
    Q_D(const QXYSeries);
    return d->m_pointLabelsFont;
}

void QXYSeries::setPointLabelsColor(const QColor &color)
{
    Q_D(QXYSeries);
    if (d->m_pointLabelsColor != color) {
        d->m_pointLabelsColor = color;
        emit pointLabelsColorChanged(color);
    }
}

QColor QXYSeries::pointLabelsColor() const
{
    Q_D(const QXYSeries);
    if (d->m_pointLabelsColor == QChartPrivate::defaultPen().color())
        return QPen().color();
    else
        return d->m_pointLabelsColor;
}

void QXYSeries::setPointLabelsClipping(bool enabled)
{
    Q_D(QXYSeries);
    if (d->m_pointLabelsClipping != enabled) {
        d->m_pointLabelsClipping = enabled;
        emit pointLabelsClippingChanged(enabled);
    }
}

bool QXYSeries::pointLabelsClipping() const
{
    Q_D(const QXYSeries);
    return d->m_pointLabelsClipping;
}

/*!
    Stream operator for adding a data \a point to the series.
    \sa append()
*/
QXYSeries &QXYSeries::operator<< (const QPointF &point)
{
    append(point);
    return *this;
}


/*!
    Stream operator for adding a list of \a points to the series.
    \sa append()
*/

QXYSeries &QXYSeries::operator<< (const QList<QPointF>& points)
{
    append(points);
    return *this;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


QXYSeriesPrivate::QXYSeriesPrivate(QXYSeries *q)
    : QAbstractSeriesPrivate(q),
      m_pen(QChartPrivate::defaultPen()),
      m_brush(QChartPrivate::defaultBrush()),
      m_pointsVisible(false),
      m_pointLabelsFormat(QLatin1String("@xPoint, @yPoint")),
      m_pointLabelsVisible(false),
      m_pointLabelsFont(QChartPrivate::defaultFont()),
      m_pointLabelsColor(QChartPrivate::defaultPen().color()),
      m_pointLabelsClipping(true)
{
}

void QXYSeriesPrivate::initializeDomain()
{
    qreal minX(0);
    qreal minY(0);
    qreal maxX(1);
    qreal maxY(1);

    Q_Q(QXYSeries);

    const QVector<QPointF> &points = q->pointsVector();

    if (!points.isEmpty()) {
        minX = points[0].x();
        minY = points[0].y();
        maxX = minX;
        maxY = minY;

        for (int i = 0; i < points.count(); i++) {
            qreal x = points[i].x();
            qreal y = points[i].y();
            minX = qMin(minX, x);
            minY = qMin(minY, y);
            maxX = qMax(maxX, x);
            maxY = qMax(maxY, y);
        }
    }

    domain()->setRange(minX, maxX, minY, maxY);
}

QList<QLegendMarker*> QXYSeriesPrivate::createLegendMarkers(QLegend* legend)
{
    Q_Q(QXYSeries);
    QList<QLegendMarker*> list;
    return list << new QXYLegendMarker(q,legend);
}

void QXYSeriesPrivate::initializeAxes()
{

}

QAbstractAxis::AxisType QXYSeriesPrivate::defaultAxisType(Qt::Orientation orientation) const
{
    Q_UNUSED(orientation);
    return QAbstractAxis::AxisTypeValue;
}

QAbstractAxis* QXYSeriesPrivate::createDefaultAxis(Qt::Orientation orientation) const
{
    Q_UNUSED(orientation);
    return new QValueAxis;
}

void QXYSeriesPrivate::initializeAnimations(QtCharts::QChart::AnimationOptions options,
                                            int duration, QEasingCurve &curve)
{
    XYChart *item = static_cast<XYChart *>(m_item.data());
    Q_ASSERT(item);
    if (item->animation())
        item->animation()->stopAndDestroyLater();

    if (options.testFlag(QChart::SeriesAnimations))
        item->setAnimation(new XYAnimation(item, duration, curve));
    else
        item->setAnimation(0);
    QAbstractSeriesPrivate::initializeAnimations(options, duration, curve);
}

void QXYSeriesPrivate::drawSeriesPointLabels(QPainter *painter, const QVector<QPointF> &points,
                                             const int offset)
{
    static const QString xPointTag(QLatin1String("@xPoint"));
    static const QString yPointTag(QLatin1String("@yPoint"));
    const int labelOffset = offset + 2;

    painter->setFont(m_pointLabelsFont);
    painter->setPen(QPen(m_pointLabelsColor));
    QFontMetrics fm(painter->font());
    // m_points is used for the label here as it has the series point information
    // points variable passed is used for positioning because it has the coordinates
    for (int i(0); i < m_points.size(); i++) {
        QString pointLabel = m_pointLabelsFormat;
        pointLabel.replace(xPointTag, presenter()->numberToString(m_points.at(i).x()));
        pointLabel.replace(yPointTag, presenter()->numberToString(m_points.at(i).y()));

        // Position text in relation to the point
        int pointLabelWidth = fm.width(pointLabel);
        QPointF position(points.at(i));
        position.setX(position.x() - pointLabelWidth / 2);
        position.setY(position.y() - labelOffset);

        painter->drawText(position, pointLabel);
    }
}

#include "moc_qxyseries.cpp"
#include "moc_qxyseries_p.cpp"

QT_CHARTS_END_NAMESPACE
