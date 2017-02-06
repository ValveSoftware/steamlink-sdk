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

#include <QtCharts/QAbstractAxis>
#include <private/qabstractaxis_p.h>
#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>
#include <private/qchart_p.h>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QAbstractAxis
    \inmodule Qt Charts
    \brief The QAbstractAxis class is a base class used for specialized axis classes.

    Each series can be bound to one or more horizontal and vertical axes, but mixing axis types
    that would result in different domains is not supported, such as specifying
    QValueAxis and QLogValueAxis on the same orientation.

    The properties and visibility of various axis elements, such as axis line, title, labels,
    grid lines, and shades, can be individually controlled.
*/
/*!
    \qmltype AbstractAxis
    \instantiates QAbstractAxis
    \inqmlmodule QtCharts

    \brief A base type used for specialized axis types.

    Each series can be bound to only one horizontal and vertical axis.

    The properties and visibility of various axis elements, such as axis line, title, labels,
    grid lines, and shades, can be individually controlled.
*/

/*!
    \enum QAbstractAxis::AxisType

    This enum type specifies the type of the axis object.

    \value AxisTypeNoAxis
    \value AxisTypeValue
    \value AxisTypeBarCategory
    \value AxisTypeCategory
    \value AxisTypeDateTime
    \value AxisTypeLogValue
*/

/*!
  \fn void QAbstractAxis::type() const
  Returns the type of the axis.
*/

/*!
  \property QAbstractAxis::lineVisible
  \brief The visibility of the axis line.
*/
/*!
  \qmlproperty bool AbstractAxis::lineVisible
  The visibility of the axis line.
*/

/*!
  \property QAbstractAxis::linePen
  \brief The pen used to draw the line.
*/

/*!
  \property QAbstractAxis::labelsVisible
  \brief Whether axis labels are visible.
*/
/*!
  \qmlproperty bool AbstractAxis::labelsVisible
  The visibility of axis labels.
*/

/*!
  \property QAbstractAxis::labelsBrush
  \brief The brush used to draw the labels.

  Only the color of the brush is relevant.
*/

/*!
  \property QAbstractAxis::visible
  \brief The visibility of the axis.
*/
/*!
  \qmlproperty bool AbstractAxis::visible
  The visibility of the axis.
*/

/*!
  \property QAbstractAxis::gridVisible
  \brief The visibility of the grid lines.
*/
/*!
  \qmlproperty bool AbstractAxis::gridVisible
  The visibility of the grid lines.
*/

/*!
  \property QAbstractAxis::minorGridVisible
  \brief The visibility of the minor grid lines.

  Applies only to QValueAxis.
*/
/*!
  \qmlproperty bool AbstractAxis::minorGridVisible
  The visibility of the minor grid lines. Applies only to ValueAxis.
*/

/*!
  \property QAbstractAxis::color
  \brief The color of the axis and tick marks.
*/
/*!
  \qmlproperty color AbstractAxis::color
  The color of the axis and tick marks.
*/

/*!
  \property QAbstractAxis::gridLinePen
  \brief The pen used to draw the grid line.
*/

/*!
  \property QAbstractAxis::minorGridLinePen
  \brief The pen used to draw the minor grid line.

  Applies only to QValueAxis.
*/

/*!
  \property QAbstractAxis::gridLineColor
  \brief The color of the grid line.
*/

/*!
  \property QAbstractAxis::minorGridLineColor
  \brief The color of the minor grid line.

  Applies only to QValueAxis.
*/

/*!
  \property QAbstractAxis::labelsFont
  \brief The font of the axis labels.
*/

/*!
  \qmlproperty font AbstractAxis::labelsFont
  The font of the axis labels.

  For more information, see \l [QML]{font}.
*/

/*!
  \property QAbstractAxis::labelsColor
  \brief The color of the axis labels.
*/
/*!
  \qmlproperty color AbstractAxis::labelsColor
  The color of the axis labels.
*/

/*!
  \property QAbstractAxis::labelsAngle
  \brief The angle of the axis labels in degrees.
*/
/*!
  \qmlproperty int AbstractAxis::labelsAngle
  The angle of the axis labels in degrees.
*/

/*!
  \property QAbstractAxis::shadesVisible
  \brief The visibility of the axis shades.
*/
/*!
  \qmlproperty bool AbstractAxis::shadesVisible
  The visibility of the axis shades.
*/

/*!
  \property QAbstractAxis::shadesColor
  \brief The fill (brush) color of the axis shades.
*/
/*!
  \qmlproperty color AbstractAxis::shadesColor
  The fill (brush) color of the axis shades.
*/

/*!
  \property QAbstractAxis::shadesBorderColor
  \brief The border (pen) color of the axis shades.
*/
/*!
  \qmlproperty color AbstractAxis::shadesBorderColor
  The border (pen) color of the axis shades.
*/

/*!
  \property QAbstractAxis::shadesPen
  \brief The pen used to draw the axis shades (the area between the grid lines).
*/

/*!
  \property QAbstractAxis::shadesBrush
  \brief The brush used to draw the axis shades (the area between the grid lines).
*/

/*!
  \property QAbstractAxis::titleVisible
  \brief The visibility of the axis title.

    By default, the value is \c true.
*/
/*!
  \qmlproperty bool AbstractAxis::titleVisible
  The visibility of the axis title. By default, the value is \c true.
*/

/*!
  \property QAbstractAxis::titleText
  \brief The title of the axis.

  Empty by default. Axis titles support HTML formatting.
*/
/*!
  \qmlproperty string AbstractAxis::titleText
  The title of the axis. Empty by default. Axis titles support HTML formatting.
*/

/*!
  \property QAbstractAxis::titleBrush
  \brief The brush used to draw the title text.

  Only the color of the brush is relevant.
*/

/*!
  \property QAbstractAxis::titleFont
  \brief The font of the title of the axis.
*/
/*!
  \qmlproperty font AbstractAxis::titleFont
  The font of the title of the axis.
*/

/*!
  \property QAbstractAxis::orientation
  \brief The orientation of the axis.

  Fixed to either Qt::Horizontal or Qt::Vertical when the axis is added to a chart.
*/
/*!
  \qmlproperty Qt.Orientation AbstractAxis::orientation
  The orientation of the axis. Fixed to either \l {Qt::Horizontal}{Qt.Horizontal}
  or \l{Qt::Vertical}{Qt.Vertical} when the axis is set to a series.
*/

/*!
  \property QAbstractAxis::alignment
  \brief The alignment of the axis.

  Can be Qt::AlignLeft, Qt::AlignRight, Qt::AlignBottom, or Qt::AlignTop.
*/
/*!
  \qmlproperty alignment AbstractAxis::alignment
  The alignment of the axis. Can be \l{Qt::AlignLeft}{Qt.AlignLeft},
  \l{Qt::AlignRight}{Qt.AlignRight}, \l{Qt::AlignBottom}{Qt.AlignBottom}, or
  \l{Qt::AlignTop}{Qt.AlignTop}.
*/

/*!
  \property QAbstractAxis::reverse
  \brief Whether a reverse axis is used.

  By default, the value is \c false.

  The reverse axis is supported with a line, spline, and scatter series, as well as an area series
  with a cartesian chart. All axes of the same orientation attached to the same series must be
  reversed if one is reversed or the behavior is undefined.
*/
/*!
  \qmlproperty alignment AbstractAxis::reverse
  Defines whether a reverse axis is used. By default, the value is \c false.

  The reverse axis is supported with a line, spline, and scatter series, as well as an area series
  with a cartesian chart. All axes of the same orientation attached to the same series must be
  reversed if one is reversed or the behavior is undefined.
*/

/*!
  \fn void QAbstractAxis::visibleChanged(bool visible)
  This signal is emitted when the visibility of the axis changes to \a visible.
*/

/*!
  \fn void QAbstractAxis::linePenChanged(const QPen& pen)
  This signal is emitted when the pen used to draw the line of the axis changes to \a pen.
*/

/*!
  \fn void QAbstractAxis::lineVisibleChanged(bool visible)
  This signal is emitted when the visibility of the axis line changes to \a visible.
*/

/*!
  \fn void QAbstractAxis::labelsVisibleChanged(bool visible)
  This signal is emitted when the visibility of the labels of the axis changes to \a visible.
*/

/*!
  \fn void QAbstractAxis::labelsFontChanged(const QFont& font)
  This signal is emitted when the font of the axis labels changes to \a font.
*/

/*!
  \fn void QAbstractAxis::labelsBrushChanged(const QBrush& brush)
  This signal is emitted when the brush used to draw the axis labels changes to \a brush.
*/

/*!
  \fn void QAbstractAxis::labelsAngleChanged(int angle)
  This signal is emitted when the angle of the axis labels changes to \a angle.
*/

/*!
  \fn void QAbstractAxis::gridVisibleChanged(bool visible)
  This signal is emitted when the visibility of the grid lines of the axis changes to \a visible.
*/

/*!
  \fn void QAbstractAxis::minorGridVisibleChanged(bool visible)
  This signal is emitted when the visibility of the minor grid lines of the axis
  changes to \a visible.
*/

/*!
  \fn void QAbstractAxis::gridLinePenChanged(const QPen& pen)
  This signal is emitted when the pen used to draw the grid line changes to \a pen.
*/

/*!
  \fn void QAbstractAxis::minorGridLinePenChanged(const QPen& pen)
  This signal is emitted when the pen used to draw the minor grid line changes to \a pen.
*/

/*!
  \fn void QAbstractAxis::gridLineColorChanged(const QColor &color)
  This signal is emitted when the color of the pen used to draw the grid line changes to \a color.
*/

/*!
  \fn void QAbstractAxis::minorGridLineColorChanged(const QColor &color)
  This signal is emitted when the color of the pen used to draw the minor grid line changes
  to \a color.
*/

/*!
  \fn void QAbstractAxis::colorChanged(QColor color)
  This signal is emitted when the color of the axis changes to \a color.
*/

/*!
  \fn void QAbstractAxis::labelsColorChanged(QColor color)
  This signal is emitted when the color of the axis labels changes to \a color.
*/

/*!
  \fn void QAbstractAxis::titleVisibleChanged(bool visible)
  This signal is emitted when the visibility of the title text of the axis changes to \a visible.
*/

/*!
  \fn void QAbstractAxis::titleTextChanged(const QString& text)
  This signal is emitted when the text of the axis title changes to \a text.
*/

/*!
  \fn void QAbstractAxis::titleBrushChanged(const QBrush& brush)
  This signal is emitted when the brush used to draw the axis title changes to \a brush.
*/

/*!
  \fn void QAbstractAxis::titleFontChanged(const QFont& font)
  This signal is emitted when the font of the axis title changes to \a font.
*/

/*!
  \fn void QAbstractAxis::shadesVisibleChanged(bool)
  This signal is emitted when the visibility of the axis shades changes to \a visible.
*/

/*!
  \fn void QAbstractAxis::shadesColorChanged(QColor color)
  This signal is emitted when the color of the axis shades changes to \a color.
*/

/*!
  \fn void QAbstractAxis::shadesBorderColorChanged(QColor color)
  This signal is emitted when the border color of the axis shades changes to \a color.
*/

/*!
  \fn void QAbstractAxis::shadesBrushChanged(const QBrush& brush)
  This signal is emitted when the brush used to draw the axis shades changes to \a brush.
*/

/*!
  \fn void QAbstractAxis::shadesPenChanged(const QPen& pen)
  This signal is emitted when the pen used to draw the axis shades changes to \a pen.
*/

/*!
 \internal
  Constructs a new axis object that is a child of \a parent. The ownership is taken by
  QChart when the axis is added.
*/

QAbstractAxis::QAbstractAxis(QAbstractAxisPrivate &d, QObject *parent)
    : QObject(parent),
      d_ptr(&d)
{
}

/*!
  Destructs the axis object. When the axis is added to a chart, the chart object takes ownership.
*/

QAbstractAxis::~QAbstractAxis()
{
    if (d_ptr->m_chart)
        qFatal("Still binded axis detected !");
}

/*!
  Sets the pen used to draw the axis line and tick marks to \a pen.
 */
void QAbstractAxis::setLinePen(const QPen &pen)
{
    if (d_ptr->m_axisPen != pen) {
        d_ptr->m_axisPen = pen;
        emit linePenChanged(pen);
    }
}

/*!
  Returns the pen used to draw the axis line and tick marks.
*/
QPen QAbstractAxis::linePen() const
{
    if (d_ptr->m_axisPen == QChartPrivate::defaultPen())
        return QPen();
    else
        return d_ptr->m_axisPen;
}

void QAbstractAxis::setLinePenColor(QColor color)
{
    QPen p = linePen();
    if (p.color() != color || d_ptr->m_axisPen == QChartPrivate::defaultPen()) {
        p.setColor(color);
        setLinePen(p);
        emit colorChanged(color);
    }
}

QColor QAbstractAxis::linePenColor() const
{
    return linePen().color();
}

/*!
  Determines whether the axis line and tick marks are \a visible.
 */
void QAbstractAxis::setLineVisible(bool visible)
{
    if (d_ptr->m_arrowVisible != visible) {
        d_ptr->m_arrowVisible = visible;
        emit lineVisibleChanged(visible);
    }
}

bool QAbstractAxis::isLineVisible() const
{
    return d_ptr->m_arrowVisible;
}

void QAbstractAxis::setGridLineVisible(bool visible)
{
    if (d_ptr->m_gridLineVisible != visible) {
        d_ptr->m_gridLineVisible = visible;
        emit gridVisibleChanged(visible);
    }
}

bool QAbstractAxis::isGridLineVisible() const
{
    return d_ptr->m_gridLineVisible;
}

void QAbstractAxis::setMinorGridLineVisible(bool visible)
{
    if (d_ptr->m_minorGridLineVisible != visible) {
        d_ptr->m_minorGridLineVisible = visible;
        emit minorGridVisibleChanged(visible);
    }
}

bool QAbstractAxis::isMinorGridLineVisible() const
{
    return d_ptr->m_minorGridLineVisible;
}

/*!
  Sets the pen used to draw the grid lines to \a pen.
*/
void QAbstractAxis::setGridLinePen(const QPen &pen)
{
    if (d_ptr->m_gridLinePen != pen) {
        d_ptr->m_gridLinePen = pen;
        emit gridLinePenChanged(pen);
    }
}

/*!
  Returns the pen used to draw the grid.
*/
QPen QAbstractAxis::gridLinePen() const
{
    if (d_ptr->m_gridLinePen == QChartPrivate::defaultPen())
        return QPen();
    else
        return d_ptr->m_gridLinePen;
}

void QAbstractAxis::setMinorGridLinePen(const QPen &pen)
{
    if (d_ptr->m_minorGridLinePen != pen) {
        d_ptr->m_minorGridLinePen = pen;
        emit minorGridLinePenChanged(pen);
    }
}

QPen QAbstractAxis::minorGridLinePen() const
{
    if (d_ptr->m_minorGridLinePen == QChartPrivate::defaultPen())
        return QPen();
    else
        return d_ptr->m_minorGridLinePen;
}

void QAbstractAxis::setGridLineColor(const QColor &color)
{
    QPen pen = gridLinePen();
    if (color != pen.color() || d_ptr->m_gridLinePen == QChartPrivate::defaultPen()) {
        pen.setColor(color);
        setGridLinePen(pen);
        emit gridLineColorChanged(color);
    }
}

QColor QAbstractAxis::gridLineColor()
{
    return gridLinePen().color();
}

void QAbstractAxis::setMinorGridLineColor(const QColor &color)
{
    QPen pen = minorGridLinePen();
    if (color != pen.color() || d_ptr->m_minorGridLinePen == QChartPrivate::defaultPen()) {
        pen.setColor(color);
        setMinorGridLinePen(pen);
        emit minorGridLineColorChanged(color);
    }
}

QColor QAbstractAxis::minorGridLineColor()
{
    return minorGridLinePen().color();
}

void QAbstractAxis::setLabelsVisible(bool visible)
{
    if (d_ptr->m_labelsVisible != visible) {
        d_ptr->m_labelsVisible = visible;
        emit labelsVisibleChanged(visible);
    }
}

bool QAbstractAxis::labelsVisible() const
{
    return d_ptr->m_labelsVisible;
}

/*!
  Sets the brush used to draw labels to \a brush.
 */
void QAbstractAxis::setLabelsBrush(const QBrush &brush)
{
    if (d_ptr->m_labelsBrush != brush) {
        d_ptr->m_labelsBrush = brush;
        emit labelsBrushChanged(brush);
    }
}

/*!
  Returns the brush used to draw labels.
*/
QBrush  QAbstractAxis::labelsBrush() const
{
    if (d_ptr->m_labelsBrush == QChartPrivate::defaultBrush())
        return QBrush();
    else
        return d_ptr->m_labelsBrush;
}

/*!
  Sets the font used to draw labels to \a font.
*/
void QAbstractAxis::setLabelsFont(const QFont &font)
{
    if (d_ptr->m_labelsFont != font) {
        d_ptr->m_labelsFont = font;
        emit labelsFontChanged(font);
    }
}

/*!
  Returns the font used to draw labels.
*/
QFont QAbstractAxis::labelsFont() const
{
    if (d_ptr->m_labelsFont == QChartPrivate::defaultFont())
        return QFont();
    else
        return d_ptr->m_labelsFont;
}

void QAbstractAxis::setLabelsAngle(int angle)
{
    if (d_ptr->m_labelsAngle != angle) {
        d_ptr->m_labelsAngle = angle;
        emit labelsAngleChanged(angle);
    }
}

int QAbstractAxis::labelsAngle() const
{
    return d_ptr->m_labelsAngle;
}
void QAbstractAxis::setLabelsColor(QColor color)
{
    QBrush b = labelsBrush();
    if (b.color() != color || d_ptr->m_labelsBrush == QChartPrivate::defaultBrush()) {
        b.setColor(color);
        setLabelsBrush(b);
        emit labelsColorChanged(color);
    }
}

QColor QAbstractAxis::labelsColor() const
{
    return labelsBrush().color();
}

void QAbstractAxis::setTitleVisible(bool visible)
{
    if (d_ptr->m_titleVisible != visible) {
        d_ptr->m_titleVisible = visible;
        emit titleVisibleChanged(visible);
    }
}

bool QAbstractAxis::isTitleVisible() const
{
    return d_ptr->m_titleVisible;
}

/*!
  Sets the brush used to draw titles to \a brush.
 */
void QAbstractAxis::setTitleBrush(const QBrush &brush)
{
    if (d_ptr->m_titleBrush != brush) {
        d_ptr->m_titleBrush = brush;
        emit titleBrushChanged(brush);
    }
}

/*!
  Returns the brush used to draw titles.
*/
QBrush QAbstractAxis::titleBrush() const
{
    if (d_ptr->m_titleBrush == QChartPrivate::defaultBrush())
        return QBrush();
    else
        return d_ptr->m_titleBrush;
}

/*!
  Sets the font used to draw titles to \a font.
*/
void QAbstractAxis::setTitleFont(const QFont &font)
{
    if (d_ptr->m_titleFont != font) {
        d_ptr->m_titleFont = font;
        emit titleFontChanged(font);
    }
}

/*!
  Returns the font used to draw titles.
*/
QFont QAbstractAxis::titleFont() const
{
    if (d_ptr->m_titleFont == QChartPrivate::defaultFont())
        return QFont();
    else
        return d_ptr->m_titleFont;
}

void QAbstractAxis::setTitleText(const QString &title)
{
    if (d_ptr->m_title != title) {
        d_ptr->m_title = title;
        emit titleTextChanged(title);
    }
}

QString QAbstractAxis::titleText() const
{
    return d_ptr->m_title;
}


void QAbstractAxis::setShadesVisible(bool visible)
{
    if (d_ptr->m_shadesVisible != visible) {
        d_ptr->m_shadesVisible = visible;
        emit shadesVisibleChanged(visible);
    }
}

bool QAbstractAxis::shadesVisible() const
{
    return d_ptr->m_shadesVisible;
}

/*!
  Sets the pen used to draw shades to \a pen.
*/
void QAbstractAxis::setShadesPen(const QPen &pen)
{
    if (d_ptr->m_shadesPen != pen) {
        d_ptr->m_shadesPen = pen;
        emit shadesPenChanged(pen);
    }
}

/*!
  Returns the pen used to draw shades.
*/
QPen QAbstractAxis::shadesPen() const
{
    if (d_ptr->m_shadesPen == QChartPrivate::defaultPen())
        return QPen();
    else
        return d_ptr->m_shadesPen;
}

/*!
  Sets the brush used to draw shades to \a brush.
*/
void QAbstractAxis::setShadesBrush(const QBrush &brush)
{
    if (d_ptr->m_shadesBrush != brush) {
        d_ptr->m_shadesBrush = brush;
        emit shadesBrushChanged(brush);
    }
}

/*!
   Returns the brush used to draw shades.
*/
QBrush QAbstractAxis::shadesBrush() const
{
    if (d_ptr->m_shadesBrush == QChartPrivate::defaultBrush())
        return QBrush(Qt::SolidPattern);
    else
        return d_ptr->m_shadesBrush;
}

void QAbstractAxis::setShadesColor(QColor color)
{
    QBrush b = shadesBrush();
    if (b.color() != color || d_ptr->m_shadesBrush == QChartPrivate::defaultBrush()) {
        b.setColor(color);
        setShadesBrush(b);
        emit shadesColorChanged(color);
    }
}

QColor QAbstractAxis::shadesColor() const
{
    return shadesBrush().color();
}

void QAbstractAxis::setShadesBorderColor(QColor color)
{
    QPen p = shadesPen();
    if (p.color() != color || d_ptr->m_shadesPen == QChartPrivate::defaultPen()) {
        p.setColor(color);
        setShadesPen(p);
        emit shadesColorChanged(color);
    }
}

QColor QAbstractAxis::shadesBorderColor() const
{
    return shadesPen().color();
}


bool QAbstractAxis::isVisible() const
{
    return d_ptr->m_visible;
}

/*!
  Sets the visibility of the axis, shades, labels, and grid lines to \a visible.
*/
void QAbstractAxis::setVisible(bool visible)
{
    if (d_ptr->m_visible != visible) {
        d_ptr->m_visible = visible;
        emit visibleChanged(visible);
    }
}


/*!
  Makes the axis, shades, labels, and grid lines visible.
*/
void QAbstractAxis::show()
{
    setVisible(true);
}

/*!
  Makes the axis, shades, labels, and grid lines invisible.
*/
void QAbstractAxis::hide()
{
    setVisible(false);
}

/*!
  Sets the minimum value shown on the axis.
  Depending on the actual axis type, the \a min parameter is converted to the appropriate type
  of value. If the conversion is impossible, the function call does nothing.
*/
void QAbstractAxis::setMin(const QVariant &min)
{
    d_ptr->setMin(min);
}

/*!
  Sets the maximum value shown on the axis.
  Depending on the actual axis type, the \a max parameter is converted to the appropriate type
  of value. If the conversion is impossible, the function call does nothing.
*/
void QAbstractAxis::setMax(const QVariant &max)
{
    d_ptr->setMax(max);
}

/*!
  Sets the range shown on the axis.
  Depending on the actual axis type, the \a min and \a max parameters are converted to
  appropriate types of values. If the conversion is impossible, the function call does nothing.
*/
void QAbstractAxis::setRange(const QVariant &min, const QVariant &max)
{
    d_ptr->setRange(min, max);
}


/*!
  Returns the orientation of the axis (vertical or horizontal).
*/
Qt::Orientation QAbstractAxis::orientation() const
{
    return d_ptr->orientation();
}

Qt::Alignment QAbstractAxis::alignment() const
{
    return d_ptr->alignment();
}

bool QAbstractAxis::isReverse() const
{
    return d_ptr->m_reverse;
}

void QAbstractAxis::setReverse(bool reverse)
{
    if (d_ptr->m_reverse != reverse && type() != QAbstractAxis::AxisTypeBarCategory) {
        d_ptr->m_reverse = reverse;
        emit reverseChanged(reverse);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QAbstractAxisPrivate::QAbstractAxisPrivate(QAbstractAxis *q)
    : q_ptr(q),
      m_chart(0),
      m_alignment(0),
      m_orientation(Qt::Orientation(0)),
      m_visible(true),
      m_arrowVisible(true),
      m_axisPen(QChartPrivate::defaultPen()),
      m_axisBrush(QChartPrivate::defaultBrush()),
      m_gridLineVisible(true),
      m_gridLinePen(QChartPrivate::defaultPen()),
      m_minorGridLineVisible(true),
      m_minorGridLinePen(QChartPrivate::defaultPen()),
      m_labelsVisible(true),
      m_labelsBrush(QChartPrivate::defaultBrush()),
      m_labelsFont(QChartPrivate::defaultFont()),
      m_labelsAngle(0),
      m_titleVisible(true),
      m_titleBrush(QChartPrivate::defaultBrush()),
      m_titleFont(QChartPrivate::defaultFont()),
      m_shadesVisible(false),
      m_shadesPen(QChartPrivate::defaultPen()),
      m_shadesBrush(QChartPrivate::defaultBrush()),
      m_shadesOpacity(1.0),
      m_dirty(false),
      m_reverse(false)
{
}

QAbstractAxisPrivate::~QAbstractAxisPrivate()
{
}

void QAbstractAxisPrivate::setAlignment( Qt::Alignment alignment)
{
    switch(alignment) {
        case Qt::AlignTop:
        case Qt::AlignBottom:
        m_orientation = Qt::Horizontal;
        break;
        case Qt::AlignLeft:
        case Qt::AlignRight:
        m_orientation = Qt::Vertical;
        break;
        default:
        qWarning()<<"No alignment specified !";
        break;
    };
    m_alignment=alignment;
}

void QAbstractAxisPrivate::initializeTheme(ChartTheme* theme, bool forced)
{
    if (forced || QChartPrivate::defaultPen() == m_axisPen)
        q_ptr->setLinePen(theme->axisLinePen());

    if (forced || QChartPrivate::defaultPen() == m_gridLinePen)
        q_ptr->setGridLinePen(theme->gridLinePen());
    if (forced || QChartPrivate::defaultPen() == m_minorGridLinePen)
        q_ptr->setMinorGridLinePen(theme->minorGridLinePen());

    if (forced || QChartPrivate::defaultBrush() == m_labelsBrush)
        q_ptr->setLabelsBrush(theme->labelBrush());
    if (forced || QChartPrivate::defaultFont() == m_labelsFont)
        q_ptr->setLabelsFont(theme->labelFont());

    if (forced || QChartPrivate::defaultBrush() == m_titleBrush)
        q_ptr->setTitleBrush(theme->labelBrush());
    if (forced || QChartPrivate::defaultFont() == m_titleFont) {
        QFont font(m_labelsFont);
        font.setBold(true);
        q_ptr->setTitleFont(font);
    }

    if (forced || QChartPrivate::defaultBrush() == m_shadesBrush)
        q_ptr->setShadesBrush(theme->backgroundShadesBrush());
    if (forced || QChartPrivate::defaultPen() == m_shadesPen)
        q_ptr->setShadesPen(theme->backgroundShadesPen());

    bool axisX = m_orientation == Qt::Horizontal;
    if (forced && (theme->backgroundShades() == ChartTheme::BackgroundShadesBoth
            || (theme->backgroundShades() == ChartTheme::BackgroundShadesVertical && axisX)
            || (theme->backgroundShades() == ChartTheme::BackgroundShadesHorizontal && !axisX))) {
         q_ptr->setShadesVisible(true);
    } else if (forced) {
        q_ptr->setShadesVisible(false);
    }
}

void QAbstractAxisPrivate::handleRangeChanged(qreal min, qreal max)
{
    setRange(min,max);
}

void QAbstractAxisPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_UNUSED(parent);
}

void QAbstractAxisPrivate::initializeAnimations(QChart::AnimationOptions options, int duration,
                                                QEasingCurve &curve)
{
    ChartAxisElement *axis = m_item.data();
    Q_ASSERT(axis);
    if (axis->animation())
        axis->animation()->stopAndDestroyLater();

    if (options.testFlag(QChart::GridAxisAnimations))
        axis->setAnimation(new AxisAnimation(axis, duration, curve));
    else
        axis->setAnimation(0);
}



#include "moc_qabstractaxis.cpp"
#include "moc_qabstractaxis_p.cpp"

QT_CHARTS_END_NAMESPACE
