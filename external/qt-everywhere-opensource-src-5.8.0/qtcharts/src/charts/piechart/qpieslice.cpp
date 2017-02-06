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
    \brief Defines a slice in pie series.

    This object defines the properties of a single slice in a QPieSeries.

    In addition to the obvious value and label properties the user can also control
    the visual appearance of a slice. By modifying the visual appearance also means that
    the user is overriding the default appearance set by the theme.

    Note that if the user has customized slices and theme is changed all customizations will be lost.

    To enable user interaction with the pie some basic signals are provided about clicking and hovering.
*/

/*!
    \qmltype PieSlice
    \instantiates QPieSlice
    \inqmlmodule QtCharts

    \brief Defines a slice in pie series.

    PieSlice defines the properties of a single slice in a PieSeries. The element should be used
    as a child for a PieSeries. For example:
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 2

    An alternative (dynamic) method for adding slices to a PieSeries is using PieSeries.append
    method.
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 4

    In that case you may want to use PieSeries.at or PieSeries.find to access the properties of
    an individual PieSlice instance.
    \snippet qmlpiechart/qml/qmlpiechart/main.qml 5
    \sa PieSeries
*/

/*!
 \enum QPieSlice::LabelPosition

 This enum describes the position of the slice label.

 \value LabelOutside Label is outside the slice with an arm.
 \value LabelInsideHorizontal Label is centered inside the slice and laid out horizontally.
 \value LabelInsideTangential Label is centered inside the slice and rotated to be parallel to the tangential of the slice's arc.
 \value LabelInsideNormal Label is centered inside the slice rotated to be parallel to the normal of the slice's arc.
 */

/*!
    \property QPieSlice::label
    Label of the slice.
    \sa labelVisible, labelBrush, labelFont, labelArmLengthFactor
*/
/*!
    \qmlproperty string PieSlice::label
    Label (text) of the slice.
*/

/*!
    \fn void QPieSlice::labelChanged()
    This signal emitted when the slice label has been changed.
    \sa label
*/
/*!
    \qmlsignal PieSlice::onLabelChanged()
    This signal emitted when the slice label has been changed.
    \sa label
*/

/*!
    \property QPieSlice::value
    Value of the slice.
    Note that if users sets a negative value it is converted to a positive value.
    \sa percentage(), QPieSeries::sum()
*/
/*!
    \qmlproperty real PieSlice::value
    Value of the slice. Note that if users sets a negative value it is converted to a positive value.
*/

/*!
    \fn void QPieSlice::valueChanged()
    This signal is emitted when the slice value changes.
    \sa value
*/
/*!
    \qmlsignal PieSlice::onValueChanged()
    This signal is emitted when the slice value changes.
    \sa value
*/

/*!
    \property QPieSlice::labelVisible
    Defines the visibility of slice label. By default the label is not visible.
    \sa label, labelBrush, labelFont, labelArmLengthFactor
*/
/*!
    \qmlproperty bool PieSlice::labelVisible
    Defines the visibility of slice label. By default the label is not visible.
*/

/*!
    \fn void QPieSlice::labelVisibleChanged()
    This signal emitted when visibility of the slice label has changed.
    \sa labelVisible
*/
/*!
    \qmlsignal PieSlice::onLabelVisibleChanged()
    This signal emitted when visibility of the slice label has changed.
    \sa labelVisible
*/

/*!
    \property QPieSlice::exploded
    If set to true the slice is "exploded" away from the pie.
    \sa explodeDistanceFactor
*/
/*!
    \qmlproperty bool PieSlice::exploded
    If set to true the slice is "exploded" away from the pie.
    \sa explodeDistanceFactor
*/

/*!
    \property QPieSlice::pen
    Pen used to draw the slice border.
*/

/*!
    \fn void QPieSlice::penChanged()
    This signal is emitted when the pen of the slice has changed.
    \sa pen
*/

/*!
    \property QPieSlice::borderColor
    Color used to draw the slice border.
    This is a convenience property for modifying the slice pen.
    \sa pen, borderWidth
*/
/*!
    \qmlproperty color PieSlice::borderColor
    Color used to draw the slice border (pen color).
    \sa borderWidth
*/

/*!
    \fn void QPieSlice::borderColorChanged()
    This signal is emitted when slice border color changes.
    \sa pen, borderColor
*/
/*!
    \qmlsignal PieSlice::onBorderColorChanged()
    This signal is emitted when slice border color changes.
    \sa borderColor
*/

/*!
    \property QPieSlice::borderWidth
    Width of the slice border.
    This is a convenience property for modifying the slice pen.
    \sa pen, borderColor
*/
/*!
    \qmlproperty int PieSlice::borderWidth
    Width of the slice border.
    This is a convenience property for modifying the slice pen.
    \sa borderColor
*/

/*!
    \fn void QPieSlice::borderWidthChanged()
    This signal is emitted when slice border width changes.
    \sa pen, borderWidth
*/
/*!
    \qmlsignal PieSlice::onBorderWidthChanged()
    This signal is emitted when slice border width changes.
    \sa borderWidth
*/

/*!
    \property QPieSlice::brush
    Brush used to draw the slice.
*/

/*!
    \fn void QPieSlice::brushChanged()
    This signal is emitted when the brush of the slice has changed.
    \sa brush
*/

/*!
    \qmlproperty QString PieSlice::brushFilename
    The name of the file used as a brush for the slice.
*/

/*!
    \property QPieSlice::color
    Fill (brush) color of the slice.
    This is a convenience property for modifying the slice brush.
    \sa brush
*/
/*!
    \qmlproperty color PieSlice::color
    Fill (brush) color of the slice.
*/

/*!
    \fn void QPieSlice::colorChanged()
    This signal is emitted when slice color changes.
    \sa brush
*/
/*!
    \qmlsignal PieSlice::onColorChanged()
    This signal is emitted when slice color changes.
*/

/*!
    \property QPieSlice::labelBrush
    Brush used to draw label and label arm of the slice.
    \sa label, labelVisible, labelFont, labelArmLengthFactor
*/

/*!
    \fn void QPieSlice::labelBrushChanged()
    This signal is emitted when the label brush of the slice has changed.
    \sa labelBrush
*/

/*!
    \property QPieSlice::labelColor
    Color used to draw the slice label.
    This is a convenience property for modifying the slice label brush.
    \sa labelBrush
*/
/*!
    \qmlproperty color PieSlice::labelColor
    Color used to draw the slice label.
*/

/*!
    \fn void QPieSlice::labelColorChanged()
    This signal is emitted when slice label color changes.
    \sa labelColor
*/
/*!
    \qmlsignal PieSlice::onLabelColorChanged()
    This signal is emitted when slice label color changes.
    \sa labelColor
*/

/*!
    \property QPieSlice::labelFont
    Font used for drawing label text.
    \sa label, labelVisible, labelArmLengthFactor
*/

/*!
    \fn void QPieSlice::labelFontChanged()
    This signal is emitted when the label font of the slice has changed.
    \sa labelFont
*/

/*!
    \qmlproperty Font PieSlice::labelFont

    Defines the font used for slice label.

    See the Qt documentation for more details of Font.

    \sa labelVisible, labelPosition
*/

/*!
    \property QPieSlice::labelPosition
    Position of the slice label.
    \sa label, labelVisible
*/
/*!
    \qmlproperty LabelPosition PieSlice::labelPosition
    Position of the slice label. One of PieSlice.LabelOutside, PieSlice.LabelInsideHorizontal,
    PieSlice.LabelInsideTangential or PieSlice.LabelInsideNormal. By default the position is
    PieSlice.LabelOutside.
    \sa labelVisible
*/

/*!
    \property QPieSlice::labelArmLengthFactor
    Defines the length of the label arm.
    The factor is relative to pie radius. For example:
    1.0 means the length is the same as the radius.
    0.5 means the length is half of the radius.
    By default the arm length is 0.15
    \sa label, labelVisible, labelBrush, labelFont
*/
/*!
    \qmlproperty real PieSlice::labelArmLengthFactor
    Defines the length of the label arm.
    The factor is relative to pie radius. For example:
    1.0 means the length is the same as the radius.
    0.5 means the length is half of the radius.
    By default the arm length is 0.15
    \sa labelVisible
*/

/*!
    \property QPieSlice::explodeDistanceFactor
    When the slice is exploded this factor defines how far the slice is exploded away from the pie.
    The factor is relative to pie radius. For example:
    1.0 means the distance is the same as the radius.
    0.5 means the distance is half of the radius.
    By default the distance is is 0.15
    \sa exploded
*/
/*!
    \qmlproperty real PieSlice::explodeDistanceFactor
    When the slice is exploded this factor defines how far the slice is exploded away from the pie.
    The factor is relative to pie radius. For example:
    1.0 means the distance is the same as the radius.
    0.5 means the distance is half of the radius.
    By default the distance is is 0.15
    \sa exploded
*/

/*!
    \property QPieSlice::percentage
    Percentage of the slice compared to the sum of all slices in the series.
    The actual value ranges from 0.0 to 1.0.
    Updated automatically once the slice is added to the series.
    \sa value, QPieSeries::sum
*/
/*!
    \qmlproperty real PieSlice::percentage
    Percentage of the slice compared to the sum of all slices in the series.
    The actual value ranges from 0.0 to 1.0.
    Updated automatically once the slice is added to the series.
*/

/*!
    \fn void QPieSlice::percentageChanged()
    This signal is emitted when the percentage of the slice has changed.
    \sa percentage
*/
/*!
    \qmlsignal void PieSlice::onPercentageChanged()
    This signal is emitted when the percentage of the slice has changed.
    \sa percentage
*/

/*!
    \property QPieSlice::startAngle
    Defines the starting angle of this slice in the series it belongs to.
    Full pie is 360 degrees where 0 degrees is at 12 a'clock.
    Updated automatically once the slice is added to the series.
*/
/*!
    \qmlproperty real PieSlice::startAngle
    Defines the starting angle of this slice in the series it belongs to.
    Full pie is 360 degrees where 0 degrees is at 12 a'clock.
    Updated automatically once the slice is added to the series.
*/

/*!
    \fn void QPieSlice::startAngleChanged()
    This signal is emitted when the starting angle f the slice has changed.
    \sa startAngle
*/
/*!
    \qmlsignal PieSlice::onStartAngleChanged()
    This signal is emitted when the starting angle f the slice has changed.
    \sa startAngle
*/

/*!
    \property QPieSlice::angleSpan
    Span of the slice in degrees.
    Full pie is 360 degrees where 0 degrees is at 12 a'clock.
    Updated automatically once the slice is added to the series.
*/
/*!
    \qmlproperty real PieSlice::angleSpan
    Span of the slice in degrees.
    Full pie is 360 degrees where 0 degrees is at 12 a'clock.
    Updated automatically once the slice is added to the series.
*/

/*!
    \fn void QPieSlice::angleSpanChanged()
    This signal is emitted when the angle span of the slice has changed.
    \sa angleSpan
*/
/*!
    \qmlsignal PieSlice::onAngleSpanChanged()
    This signal is emitted when the angle span of the slice has changed.
    \sa angleSpan
*/

/*!
    \fn void QPieSlice::clicked()
    This signal is emitted when user has clicked the slice.
    \sa QPieSeries::clicked()
*/
/*!
    \qmlsignal PieSlice::onClicked()
    This signal is emitted when user has clicked the slice.
*/

/*!
    \fn void QPieSlice::pressed()
    This signal is emitted when user has pressed the slice.
    \sa QPieSeries::pressed()
*/
/*!
    \qmlsignal PieSlice::onPressed()
    This signal is emitted when user has pressed the slice.
*/

/*!
    \fn void QPieSlice::released()
    This signal is emitted when user has released the slice.
    \sa QPieSeries::released()
*/
/*!
    \qmlsignal PieSlice::onReleased()
    This signal is emitted when user has released the slice.
*/

/*!
    \fn void QPieSlice::doubleClicked()
    This signal is emitted when user has doubleclicked the slice.
    \sa QPieSeries::doubleClicked()
*/
/*!
    \qmlsignal PieSlice::onDoubleClicked()
    This signal is emitted when user has doubleclicked the slice.
*/

/*!
    \fn void QPieSlice::hovered(bool state)
    This signal is emitted when user has hovered over or away from the slice.
    \a state is true when user has hovered over the slice and false when hover has moved away from the slice.
    \sa QPieSeries::hovered()
*/
/*!
    \qmlsignal PieSlice::onHovered(bool state)
    This signal is emitted when user has hovered over or away from the slice.
    \a state is true when user has hovered over the slice and false when hover has moved away from the slice.
*/

/*!
    Constructs an empty slice with a \a parent.
    \sa QPieSeries::append(), QPieSeries::insert()
*/
QPieSlice::QPieSlice(QObject *parent)
    : QObject(parent),
      d_ptr(new QPieSlicePrivate(this))
{

}

/*!
    Constructs an empty slice with given \a value, \a label and a \a parent.
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
    Destroys the slice.
    User should not delete the slice if it has been added to the series.
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
