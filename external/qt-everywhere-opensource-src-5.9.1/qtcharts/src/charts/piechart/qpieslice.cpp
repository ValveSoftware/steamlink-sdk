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

#include <QtCharts/QPieSlice>
#include <private/qpieslice_p.h>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QPieSlice
    \inmodule Qt Charts
    \brief The QPieSlice class represents a single slice in a pie series.

    A pie slice has a value and a label. When the slice is added to a pie series, the
    QPieSeries object calculates the percentage of the slice compared with the sum of
    all slices in the series to determine the actual size of the slice in the chart.

    By default, the label is hidden. If it is visible, it can be either located outside
    the slice and connected to it with an arm or centered inside the slice either
    horizontally or in parallel with the tangential or normal of the slice's arc.

    By default, the visual appearance of the slice is set by a theme, but the theme can be
    overridden by specifying slice properties. However, if the theme is changed after the
    slices are customized, all customization will be lost.

    To enable user interaction with the pie chart, some basic signals are emitted when
    users click pie slices or hover the mouse over them.

    \sa QPieSeries
*/

/*!
    \qmltype PieSlice
    \instantiates QPieSlice
    \inqmlmodule QtCharts

    \brief Represents a single slice in a pie series.

    A pie slice has a value and a label. When the slice is added to a pie series, the
    PieSeries type calculates the percentage of the slice compared with the sum of
    all slices in the series to determine the actual size of the slice in the chart.

    By default, the label is hidden. If it is visible, it can be either located outside
    the slice and connected to it with an arm or centered inside the slice either
    horizontally or in parallel with the tangential or normal of the slice's arc.

    By default, the visual appearance of the slice is set by a theme, but the theme can be
    overridden by specifying slice properties. However, if the theme is changed after the
    slices are customized, all customization will be lost.

    The PieSlice type should be used as a child of a PieSeries type. For example:
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 2

    Alternatively, slices can be added to a pie series by using the \l{PieSeries::append()}
    {PieSeries.append()} method.
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 4

    In that case, \l{PieSeries::at()}{PieSeries.at()} or \l {PieSeries::find}
    {PieSeries.find} can be used to access the properties of an individual PieSlice instance.
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 5
    \sa PieSeries
*/

/*!
 \enum QPieSlice::LabelPosition

 This enum describes the position of the slice label.

    \value  LabelOutside
            The label is located outside the slice connected to it with an arm.
            This is the default value.
    \value  LabelInsideHorizontal
            The label is centered within the slice and laid out horizontally.
    \value  LabelInsideTangential
            The label is centered within the slice and rotated to be parallel with
            the tangential of the slice's arc.
    \value  LabelInsideNormal
            The label is centered within the slice and rotated to be parallel with
            the normal of the slice's arc.
 */

/*!
    \property QPieSlice::label
    \brief The label of the slice.
    \sa labelVisible, labelBrush, labelFont, labelArmLengthFactor
*/
/*!
    \qmlproperty string PieSlice::label
    The label of the slice.
*/

/*!
    \fn void QPieSlice::labelChanged()
    This signal is emitted when the slice label changes.
    \sa label
*/

/*!
    \property QPieSlice::value
    \brief The value of the slice.
    \note A negative value is converted to a positive value.
    \sa percentage(), QPieSeries::sum()
*/
/*!
    \qmlproperty real PieSlice::value
    The value of the slice.

    \note A negative value is converted to a positive value.
*/

/*!
    \fn void QPieSlice::valueChanged()
    This signal is emitted when the slice value changes.
    \sa value
*/

/*!
    \property QPieSlice::labelVisible
    \brief The visibility of the slice label. By default, the label is not visible.
    \sa label, labelBrush, labelFont, labelArmLengthFactor
*/
/*!
    \qmlproperty bool PieSlice::labelVisible
    The visibility of the slice label. By default, the label is not visible.
*/

/*!
    \fn void QPieSlice::labelVisibleChanged()
    This signal is emitted when the visibility of the slice label changes.
    \sa labelVisible
*/

/*!
    \property QPieSlice::exploded
    \brief Whether the slice is separated from the pie.
    \sa explodeDistanceFactor
*/
/*!
    \qmlproperty bool PieSlice::exploded
    Whether the slice is separated from the pie.
    \sa explodeDistanceFactor
*/

/*!
    \property QPieSlice::pen
    \brief The pen used to draw the slice border.
*/

/*!
    \fn void QPieSlice::penChanged()
    This signal is emitted when the pen used to draw the slice border changes.
    \sa pen
*/

/*!
    \property QPieSlice::borderColor
    \brief The color used to draw the slice border.
    This is a convenience property for modifying the slice pen.
    \sa pen, borderWidth
*/
/*!
    \qmlproperty color PieSlice::borderColor
    The color used to draw the slice border (pen color).
    \sa borderWidth
*/

/*!
    \fn void QPieSlice::borderColorChanged()
    This signal is emitted when the slice border color changes.
    \sa pen, borderColor
*/

/*!
    \property QPieSlice::borderWidth
    \brief The width of the slice border.
    This is a convenience property for modifying the slice pen.
    \sa pen, borderColor
*/
/*!
    \qmlproperty int PieSlice::borderWidth
    The width of the slice border.
    This is a convenience property for modifying the slice pen.
    \sa borderColor
*/

/*!
    \fn void QPieSlice::borderWidthChanged()
    This signal is emitted when the slice border width changes.
    \sa pen, borderWidth
*/

/*!
    \property QPieSlice::brush
    \brief The brush used to fill the slice.
*/

/*!
    \fn void QPieSlice::brushChanged()
    This signal is emitted when the brush used to fill the slice changes.
    \sa brush
*/

/*!
    \qmlproperty string PieSlice::brushFilename
    The name of the file used as a brush for the slice.
*/

/*!
    \property QPieSlice::color
    \brief The fill (brush) color of the slice.
    This is a convenience property for modifying the slice brush.
    \sa brush
*/
/*!
    \qmlproperty color PieSlice::color
    The fill (brush) color of the slice.
*/

/*!
    \fn void QPieSlice::colorChanged()
    This signal is emitted when the slice color changes.
    \sa brush
*/

/*!
    \property QPieSlice::labelBrush
    \brief The brush used to draw the label and label arm of the slice.
    \sa label, labelVisible, labelFont, labelArmLengthFactor
*/

/*!
    \fn void QPieSlice::labelBrushChanged()
    This signal is emitted when the label brush of the slice changes.
    \sa labelBrush
*/

/*!
    \property QPieSlice::labelColor
    \brief The color used to draw the slice label.
    This is a convenience property for modifying the slice label brush.
    \sa labelBrush
*/
/*!
    \qmlproperty color PieSlice::labelColor
    The color used to draw the slice label.
*/

/*!
    \fn void QPieSlice::labelColorChanged()
    This signal is emitted when the slice label color changes.
    \sa labelColor
*/

/*!
    \property QPieSlice::labelFont
    \brief The font used for drawing the label text.
    \sa label, labelVisible, labelArmLengthFactor
*/

/*!
    \fn void QPieSlice::labelFontChanged()
    This signal is emitted when the label font of the slice changes.
    \sa labelFont
*/

/*!
    \qmlproperty font PieSlice::labelFont

    The font used for the slice label.

    For more information, see \l [QML]{font}.

    \sa labelVisible, labelPosition
*/

/*!
    \property QPieSlice::labelPosition
    \brief The position of the slice label.
    \sa label, labelVisible
*/
/*!
    \qmlproperty enumeration PieSlice::labelPosition

    Describes the position of the slice label.

    \value  PieSlice.LabelOutside
            The label is located outside the slice connected to it with an arm.
            This is the default value.
    \value  PieSlice.LabelInsideHorizontal
            The label is centered within the slice and laid out horizontally.
    \value  PieSlice.LabelInsideTangential
            The label is centered within the slice and rotated to be parallel with
            the tangential of the slice's arc.
    \value  PieSlice.LabelInsideNormal
            The label is centered within the slice and rotated to be parallel with
            the normal of the slice's arc.

    \sa labelVisible
*/

/*!
    \property QPieSlice::labelArmLengthFactor
    \brief The length of the label arm.
    The factor is relative to the pie radius. For example:
    \list
        \li 1.0 means that the length is the same as the radius.
        \li 0.5 means that the length is half of the radius.
    \endlist
    By default, the arm length is 0.15
    \sa label, labelVisible, labelBrush, labelFont
*/
/*!
    \qmlproperty real PieSlice::labelArmLengthFactor
    The length of the label arm.
    The factor is relative to the pie radius. For example:
    \list
        \li 1.0 means that the length is the same as the radius.
        \li 0.5 means that the length is half of the radius.
    \endlist
    By default, the arm length is 0.15

    \sa labelVisible
*/

/*!
    \property QPieSlice::explodeDistanceFactor
    \brief Determines how far away from the pie the slice is exploded.
    \list
        \li 1.0 means that the distance is the same as the radius.
        \li 0.5 means that the distance is half of the radius.
    \endlist
    By default, the distance is 0.15
    \sa exploded
*/
/*!
    \qmlproperty real PieSlice::explodeDistanceFactor
    Determines how far away from the pie the slice is exploded.
    \list
        \li 1.0 means that the distance is the same as the radius.
        \li 0.5 means that the distance is half of the radius.
    \endlist
    By default, the distance is 0.15

    \sa exploded
*/

/*!
    \property QPieSlice::percentage
    \brief The percentage of the slice compared to the sum of all slices in the series.
    The actual value ranges from 0.0 to 1.0.
    Updated automatically once the slice is added to the series.
    \sa value, QPieSeries::sum
*/
/*!
    \qmlproperty real PieSlice::percentage
    The percentage of the slice compared to the sum of all slices in the series.
    The actual value ranges from 0.0 to 1.0.
    Updated automatically once the slice is added to the series.
*/

/*!
    \fn void QPieSlice::percentageChanged()
    This signal is emitted when the percentage of the slice changes.
    \sa percentage
*/

/*!
    \property QPieSlice::startAngle
    \brief The starting angle of this slice in the series it belongs to.
    A full pie is 360 degrees, where 0 degrees is at 12 a'clock.
    Updated automatically once the slice is added to the series.
*/
/*!
    \qmlproperty real PieSlice::startAngle
    The starting angle of this slice in the series it belongs to.
    A full pie is 360 degrees, where 0 degrees is at 12 a'clock.
    Updated automatically once the slice is added to the series.
*/

/*!
    \fn void QPieSlice::startAngleChanged()
    This signal is emitted when the starting angle of the slice changes.
    \sa startAngle
*/

/*!
    \property QPieSlice::angleSpan
    \brief  The span of the slice in degrees.
    A full pie is 360 degrees, where 0 degrees is at 12 a'clock.
    Updated automatically once the slice is added to the series.
*/
/*!
    \qmlproperty real PieSlice::angleSpan
    The span of the slice in degrees.
    A full pie is 360 degrees, where 0 degrees is at 12 a'clock.
    Updated automatically once the slice is added to the series.
*/

/*!
    \fn void QPieSlice::angleSpanChanged()
    This signal is emitted when the angle span of the slice changes.
    \sa angleSpan
*/

/*!
    \fn void QPieSlice::clicked()
    This signal is emitted when the slice is clicked.
    \sa QPieSeries::clicked()
*/
/*!
    \qmlsignal PieSlice::clicked()
    This signal is emitted when the slice is clicked.

    The corresponding signal handler is \c onClicked().
*/

/*!
    \fn void QPieSlice::pressed()
    This signal is emitted when the user clicks the slice and holds down the mouse button.
    \sa QPieSeries::pressed()
*/
/*!
    \qmlsignal PieSlice::pressed()
    This signal is emitted when user clicks the slice and holds down the mouse button.

    The corresponding signal handler is \c onPressed().
*/

/*!
    \fn void QPieSlice::released()
    This signal is emitted when the user releases the mouse press on the slice.
    \sa QPieSeries::released()
*/
/*!
    \qmlsignal PieSlice::released()
    This signal is emitted when the user releases the mouse press on the slice.

    The corresponding signal handler is \c onReleased().
*/

/*!
    \fn void QPieSlice::doubleClicked()
    This signal is emitted when user double-clicks the slice.
    \sa QPieSeries::doubleClicked()
*/
/*!
    \qmlsignal PieSlice::doubleClicked()
    This signal is emitted when user double-clicks the slice.

    The corresponding signal handler is \c onDoubleClicked().
*/

/*!
    \fn void QPieSlice::hovered(bool state)
    This signal is emitted when a mouse is hovered over the slice. When the mouse
    moves over the slice, \a state turns \c true, and when the mouse moves away
    again, it turns \c false.
    \sa QPieSeries::hovered()
*/
/*!
    \qmlsignal PieSlice::hovered(bool state)
    This signal is emitted when a mouse is hovered over the slice. When the mouse
    moves over the slice, \a state turns \c true, and when the mouse moves away
    again, it turns \c false.

    The corresponding signal handler is \c onHovered().
*/

/*!
    Constructs an empty slice with the parent \a parent.
    \sa QPieSeries::append(), QPieSeries::insert()
*/
QPieSlice::QPieSlice(QObject *parent)
    : QObject(parent),
      d_ptr(new QPieSlicePrivate(this))
{

}

/*!
    Constructs an empty slice with the specified \a value, \a label, and \a parent.
    \sa QPieSeries::append(), QPieSeries::insert()
*/
QPieSlice::QPieSlice(QString label, qreal value, QObject *parent)
    : QObject(parent),
      d_ptr(new QPieSlicePrivate(this))
{
    setValue(value);
    setLabel(label);
}

/*!
    Removes the slice. The slice should not be removed if it has been added to a series.
*/
QPieSlice::~QPieSlice()
{

}

void QPieSlice::setLabel(QString label)
{
    if (d_ptr->m_data.m_labelText != label) {
        d_ptr->m_data.m_labelText = label;
        emit labelChanged();
    }
}

QString QPieSlice::label() const
{
    return d_ptr->m_data.m_labelText;
}

void QPieSlice::setValue(qreal value)
{
    value = qAbs(value); // negative values not allowed
    if (!qFuzzyCompare(d_ptr->m_data.m_value, value)) {
        d_ptr->m_data.m_value = value;
        emit valueChanged();
    }
}

qreal QPieSlice::value() const
{
    return d_ptr->m_data.m_value;
}

void QPieSlice::setLabelVisible(bool visible)
{
    if (d_ptr->m_data.m_isLabelVisible != visible) {
        d_ptr->m_data.m_isLabelVisible = visible;
        emit labelVisibleChanged();
    }
}

bool QPieSlice::isLabelVisible() const
{
    return d_ptr->m_data.m_isLabelVisible;
}

void QPieSlice::setExploded(bool exploded)
{
    if (d_ptr->m_data.m_isExploded != exploded) {
        d_ptr->m_data.m_isExploded = exploded;
        emit d_ptr->explodedChanged();
    }
}

QPieSlice::LabelPosition QPieSlice::labelPosition()
{
    return d_ptr->m_data.m_labelPosition;
}

void QPieSlice::setLabelPosition(LabelPosition position)
{
    if (d_ptr->m_data.m_labelPosition != position) {
        d_ptr->m_data.m_labelPosition = position;
        emit d_ptr->labelPositionChanged();
    }
}

bool QPieSlice::isExploded() const
{
    return d_ptr->m_data.m_isExploded;
}

void QPieSlice::setPen(const QPen &pen)
{
    d_ptr->setPen(pen, false);
}

QPen QPieSlice::pen() const
{
    return d_ptr->m_data.m_slicePen;
}

QColor QPieSlice::borderColor()
{
    return pen().color();
}

void QPieSlice::setBorderColor(QColor color)
{
    QPen p = pen();
    if (color != p.color()) {
        p.setColor(color);
        setPen(p);
    }
}

int QPieSlice::borderWidth()
{
    return pen().width();
}

void QPieSlice::setBorderWidth(int width)
{
    QPen p = pen();
    if (width != p.width()) {
        p.setWidth(width);
        setPen(p);
    }
}

void QPieSlice::setBrush(const QBrush &brush)
{
    d_ptr->setBrush(brush, false);
}

QBrush QPieSlice::brush() const
{
    return d_ptr->m_data.m_sliceBrush;
}

QColor QPieSlice::color()
{
    return brush().color();
}

void QPieSlice::setColor(QColor color)
{
    QBrush b = brush();

    if (b == QBrush())
        b.setStyle(Qt::SolidPattern);
    b.setColor(color);
    setBrush(b);
}

void QPieSlice::setLabelBrush(const QBrush &brush)
{
    d_ptr->setLabelBrush(brush, false);
}

QBrush QPieSlice::labelBrush() const
{
    return d_ptr->m_data.m_labelBrush;
}

QColor QPieSlice::labelColor()
{
    return labelBrush().color();
}

void QPieSlice::setLabelColor(QColor color)
{
    QBrush b = labelBrush();
    if (color != b.color()) {
        b.setColor(color);
        setLabelBrush(b);
    }
}

void QPieSlice::setLabelFont(const QFont &font)
{
    d_ptr->setLabelFont(font, false);
}

QFont QPieSlice::labelFont() const
{
    return d_ptr->m_data.m_labelFont;
}

void QPieSlice::setLabelArmLengthFactor(qreal factor)
{
    if (!qFuzzyCompare(d_ptr->m_data.m_labelArmLengthFactor, factor)) {
        d_ptr->m_data.m_labelArmLengthFactor = factor;
        emit d_ptr->labelArmLengthFactorChanged();
    }
}

qreal QPieSlice::labelArmLengthFactor() const
{
    return d_ptr->m_data.m_labelArmLengthFactor;
}

void QPieSlice::setExplodeDistanceFactor(qreal factor)
{
    if (!qFuzzyCompare(d_ptr->m_data.m_explodeDistanceFactor, factor)) {
        d_ptr->m_data.m_explodeDistanceFactor = factor;
        emit d_ptr->explodeDistanceFactorChanged();
    }
}

qreal QPieSlice::explodeDistanceFactor() const
{
    return d_ptr->m_data.m_explodeDistanceFactor;
}

qreal QPieSlice::percentage() const
{
    return d_ptr->m_data.m_percentage;
}

qreal QPieSlice::startAngle() const
{
    return d_ptr->m_data.m_startAngle;
}

qreal QPieSlice::angleSpan() const
{
    return d_ptr->m_data.m_angleSpan;
}

/*!
  Returns the series that this slice belongs to.

  \sa QPieSeries::append()
*/
QPieSeries *QPieSlice::series() const
{
    return d_ptr->m_series;
}

QPieSlicePrivate::QPieSlicePrivate(QPieSlice *parent)
    : QObject(parent),
      q_ptr(parent),
      m_series(0)
{

}

QPieSlicePrivate::~QPieSlicePrivate()
{

}

QPieSlicePrivate *QPieSlicePrivate::fromSlice(QPieSlice *slice)
{
    return slice->d_func();
}

void QPieSlicePrivate::setPen(const QPen &pen, bool themed)
{
    if (m_data.m_slicePen != pen) {

        QPen oldPen = m_data.m_slicePen;

        m_data.m_slicePen = pen;
        m_data.m_slicePen.setThemed(themed);

        emit q_ptr->penChanged();
        if (oldPen.color() != pen.color())
            emit q_ptr->borderColorChanged();
        if (oldPen.width() != pen.width())
            emit q_ptr->borderWidthChanged();
    }
}

void QPieSlicePrivate::setBrush(const QBrush &brush, bool themed)
{
    if (m_data.m_sliceBrush != brush) {

        QBrush oldBrush = m_data.m_sliceBrush;

        m_data.m_sliceBrush = brush;
        m_data.m_sliceBrush.setThemed(themed);

        emit q_ptr->brushChanged();
        if (oldBrush.color() != brush.color())
            emit q_ptr->colorChanged();
    }
}

void QPieSlicePrivate::setLabelBrush(const QBrush &brush, bool themed)
{
    if (m_data.m_labelBrush != brush) {

        QBrush oldBrush = m_data.m_labelBrush;

        m_data.m_labelBrush = brush;
        m_data.m_labelBrush.setThemed(themed);

        emit q_ptr->labelBrushChanged();
        if (oldBrush.color() != brush.color())
            emit q_ptr->labelColorChanged();
    }
}

void QPieSlicePrivate::setLabelFont(const QFont &font, bool themed)
{
    if (m_data.m_labelFont != font) {
        m_data.m_labelFont = font;
        m_data.m_labelFont.setThemed(themed);
        emit q_ptr->labelFontChanged();
    }
}

void QPieSlicePrivate::setPercentage(qreal percentage)
{
    if (!qFuzzyCompare(m_data.m_percentage, percentage)) {
        m_data.m_percentage = percentage;
        emit q_ptr->percentageChanged();
    }
}

void QPieSlicePrivate::setStartAngle(qreal angle)
{
    if (!qFuzzyCompare(m_data.m_startAngle, angle)) {
        m_data.m_startAngle = angle;
        emit q_ptr->startAngleChanged();
    }
}

void QPieSlicePrivate::setAngleSpan(qreal span)
{
    if (!qFuzzyCompare(m_data.m_angleSpan, span)) {
        m_data.m_angleSpan = span;
        emit q_ptr->angleSpanChanged();
    }
}

QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE
#include "moc_qpieslice.cpp"
#include "moc_qpieslice_p.cpp"
