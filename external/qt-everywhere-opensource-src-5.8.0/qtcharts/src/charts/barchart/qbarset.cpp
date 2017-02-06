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

#include <QtCharts/QBarSet>
#include <private/qbarset_p.h>
#include <private/charthelpers_p.h>
#include <private/qchart_p.h>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QBarSet
    \inmodule Qt Charts
    \brief The QBarSet class represents one set of bars in a bar chart.

    A bar set contains one data value for each category. The first value of a set is assumed to
    belong to the first category, the second one to the second category, and so on. If the set has
    fewer values than there are categories, the missing values are assumed to be located at the end
    of the set. For missing values in the middle of a set, the numerical value of zero is used.
    Labels for zero value sets are not shown.

    \sa QAbstractBarSeries, QBarSeries, QStackedBarSeries, QPercentBarSeries
    \sa QHorizontalBarSeries, QHorizontalStackedBarSeries, QHorizontalPercentBarSeries
*/
/*!
    \qmltype BarSet
    \instantiates QBarSet
    \inqmlmodule QtCharts

    \brief Represents one set of bars in a bar chart.

    A bar set contains one data value for each category. The first value of a set is assumed to
    belong to the first category, the second one to the second category, and so on. If the set has
    fewer values than there are categories, the missing values are assumed to be located at the end
    of the set. For missing values in the middle of a set, the numerical value of zero is used.
    Labels for zero value sets are not shown.

    \sa AbstractBarSeries, BarSeries, StackedBarSeries, PercentBarSeries
    \sa HorizontalBarSeries, HorizontalStackedBarSeries, HorizontalPercentBarSeries

*/

/*!
    \property QBarSet::label
    \brief The label of the bar set.
*/
/*!
    \qmlproperty string BarSet::label
    The label of the bar set.
*/

/*!
    \property QBarSet::pen
    \brief The pen used to draw the lines of bars in the bar set.
*/

/*!
    \property QBarSet::brush
    \brief The brush used to fill the bars in the bar set.
*/

/*!
    \qmlproperty string BarSet::brushFilename
    The name of the file used as a brush for the set.
*/

/*!
    \property QBarSet::labelBrush
    \brief The brush used to draw the bar set's label.
*/

/*!
    \property QBarSet::labelFont
    \brief The font used to draw the bar set's label.
*/

/*!
    \qmlproperty font BarSet::labelFont
    The font used to draw the bar set's label.

    For more information, see \l [QML]{font}.
*/

/*!
    \property QBarSet::color
    \brief The fill (brush) color of the bar set.
*/
/*!
    \qmlproperty color BarSet::color
    The fill (brush) color of the bar set.
*/

/*!
    \property QBarSet::borderColor
    \brief The line (pen) color of the bar set.
*/
/*!
    \qmlproperty color BarSet::borderColor
    The line (pen) color of the bar set.
*/

/*!
    \qmlproperty real BarSet::borderWidth
    The width of the border line. By default, the width is 2.0.
*/

/*!
    \property QBarSet::labelColor
    \brief The text (label) color of the bar set.
*/
/*!
    \qmlproperty color BarSet::labelColor
    The text (label) color of the bar set.
*/

/*!
    \fn void QBarSet::clicked(int index)

    This signal is emitted when the user clicks the bar specified by \a index in a bar set.
*/

/*!
    \fn void QBarSet::pressed(int index)

    This signal is emitted when the user clicks the bar specified by \a index in a bar set
    and holds down the mouse button.
*/

/*!
    \fn void QBarSet::released(int index)

    This signal is emitted when the user releases the mouse press on the bar specified by
    \a index in a bar set.
*/

/*!
    \fn void QBarSet::doubleClicked(int index)

    This signal is emitted when the user double-clicks the bar specified by \a index in a bar set.
*/

/*!
    \fn void QBarSet::hovered(bool status, int index)

    This signal is emitted when a mouse is hovered over the bar specified by \a index in a bar set.
    When the mouse moves over the bar, \a status turns \c true, and when the mouse moves away again,
    it turns \c false.
*/


/*!
    \fn void QBarSet::labelChanged()
    This signal is emitted when the label of the bar set changes.
    \sa label
*/

/*!
    \fn void QBarSet::penChanged()
    This signal is emitted when the pen used to draw the bar set changes.
    \sa pen
*/

/*!
    \fn void QBarSet::brushChanged()
    This signal is emitted when the brush used to draw the bar set changes.
    \sa brush
*/

/*!
    \fn void QBarSet::labelBrushChanged()
    This signal is emitted when the brush used to draw the bar set's label changes.
    \sa labelBrush
*/

/*!
    \fn void QBarSet::labelFontChanged()
    This signal is emitted when the font of the bar set's label changes.
    \sa labelBrush
*/

/*!
    \fn void QBarSet::colorChanged(QColor)
    This signal is emitted when the fill (brush) color of the bar set changes to \a color.
*/

/*!
    \fn void QBarSet::borderColorChanged(QColor)
    This signal is emitted when the line (pen) color of the bar set changes to \a color.
*/

/*!
    \fn void QBarSet::labelColorChanged(QColor)
    This signal is emitted when the text (label) color of the bar set changes to \a color.
*/

/*!
    \fn void QBarSet::valuesAdded(int index, int count)
    This signal is emitted when new values are added to the bar set.
    \a index indicates the position of the first inserted value, and \a count is the number
    of inserted values.
    \sa append(), insert()
*/
/*!
    \qmlsignal BarSet::valuesAdded(int index, int count)
    This signal is emitted when new values are added to the bar set.
    \a index indicates the position of the first inserted value, and \a count is the number
    of inserted values.

    The corresponding signal handler is \c onValuesAdded.
*/

/*!
    \fn void QBarSet::valuesRemoved(int index, int count)
    This signal is emitted when values are removed from the bar set.
    \a index indicates the position of the first removed value, and \a count is the number
    of removed values.
    \sa remove()
*/
/*!
    \qmlsignal BarSet::valuesRemoved(int index, int count)
    This signal is emitted when values are removed from the bar set.
    \a index indicates the position of the first removed value, and \a count is the number
    of removed values.

    The corresponding signal handler is \c onValuesRemoved.
*/

/*!
    \fn void QBarSet::valueChanged(int index)
    This signal is emitted when the value at the position specified by \a index is modified.
    \sa at()
*/
/*!
    \qmlsignal BarSet::valueChanged(int index)
    This signal is emitted when the value at the position specified by \a index is modified.

    The corresponding signal handler is \c onValueChanged.
*/

/*!
    \qmlproperty int BarSet::count
    The number of values in the bar set.
*/

/*!
    \qmlproperty QVariantList BarSet::values
    The values of the bar set. You can set a list of either \l [QML]{real} or \l [QML]{point}
    types as values.

    If you set a list of real types as values, they directly define the bar set values.

    If you set a list of point types as values, the x-coordinate of the point specifies its
    zero-based index in the bar set. The size of the bar set is the highest x-coordinate value + 1.
    If a point is missing for any x-coordinate between zero and the highest value,
    it gets the value zero.

    For example, the following bar sets have equal values:
    \code
        myBarSet1.values = [5, 0, 1, 5];
        myBarSet2.values = [Qt.point(0, 5), Qt.point(2, 1), Qt.point(3, 5)];
    \endcode
*/

/*!
    Constructs a bar set with the label \a label and the parent \a parent.
*/
QBarSet::QBarSet(const QString label, QObject *parent)
    : QObject(parent),
      d_ptr(new QBarSetPrivate(label, this))
{
}

/*!
    Removes the bar set.
*/
QBarSet::~QBarSet()
{
    // NOTE: d_ptr destroyed by QObject
}

/*!
    Sets \a label as the new label for the bar set.
*/
void QBarSet::setLabel(const QString label)
{
    d_ptr->m_label = label;
    emit labelChanged();
}

/*!
    Returns the label of the bar set.
*/
QString QBarSet::label() const
{
    return d_ptr->m_label;
}

/*!
    \qmlmethod BarSet::append(real value)
    Appends the new value specified by \a value to the end of the bar set.
*/

/*!
    Appends the new value specified by \a value to the end of the bar set.
*/
void QBarSet::append(const qreal value)
{
    // Convert to QPointF
    int index = d_ptr->m_values.count();
    d_ptr->append(QPointF(d_ptr->m_values.count(), value));
    emit valuesAdded(index, 1);
}

/*!
    Appends the list of real values specified by \a values to the end of the bar set.

    \sa append()
*/
void QBarSet::append(const QList<qreal> &values)
{
    int index = d_ptr->m_values.count();
    d_ptr->append(values);
    emit valuesAdded(index, values.count());
}

/*!
    A convenience operator for appending the real value specified by \a value to the end of the
    bar set.

    \sa append()
*/
QBarSet &QBarSet::operator << (const qreal &value)
{
    append(value);
    return *this;
}

/*!
    Inserts \a value in the position specified by \a index.
    The values following the inserted value are moved up one position.

    \sa remove()
*/
void QBarSet::insert(const int index, const qreal value)
{
    d_ptr->insert(index, value);
    emit valuesAdded(index, 1);
}

/*!
    \qmlmethod BarSet::remove(int index, int count)
    Removes the number of values specified by \a count from the bar set starting
    with the value specified by \a index.

    If you leave out \a count, only the value specified by \a index is removed.
*/

/*!
    Removes the number of values specified by \a count from the bar set starting with
    the value specified by \a index.
    \sa insert()
*/
void QBarSet::remove(const int index, const int count)
{
    int removedCount = d_ptr->remove(index, count);
    if (removedCount > 0)
        emit valuesRemoved(index, removedCount);
    return;
}

/*!
    \qmlmethod BarSet::replace(int index, real value)
    Adds the value specified by \a value to the bar set at the position
    specified by \a index.
*/

/*!
    Adds the value specified by \a value to the bar set at the position specified by \a index.
*/
void QBarSet::replace(const int index, const qreal value)
{
    if (index >= 0 && index < d_ptr->m_values.count()) {
        d_ptr->replace(index, value);
        emit valueChanged(index);
    }
}

/*!
    \qmlmethod BarSet::at(int index)
    Returns the value specified by \a index from the bar set.
    If the index is out of bounds, 0.0 is returned.
*/

/*!
    Returns the value specified by \a index from the bar set.
    If the index is out of bounds, 0.0 is returned.
*/
qreal QBarSet::at(const int index) const
{
    if (index < 0 || index >= d_ptr->m_values.count())
        return 0;
    return d_ptr->m_values.at(index).y();
}

/*!
    Returns the value of the bar set specified by \a index.
    If the index is out of bounds, 0.0 is returned.
*/
qreal QBarSet::operator [](const int index) const
{
    return at(index);
}

/*!
    Returns the number of values in a bar set.
*/
int QBarSet::count() const
{
    return d_ptr->m_values.count();
}

/*!
    Returns the sum of all values in the bar set.
*/
qreal QBarSet::sum() const
{
    qreal total(0);
    for (int i = 0; i < d_ptr->m_values.count(); i++)
        total += d_ptr->m_values.at(i).y();
    return total;
}

/*!
    Sets the pen used to draw the lines in the bar set to \a pen.
*/
void QBarSet::setPen(const QPen &pen)
{
    if (d_ptr->m_pen != pen) {
        d_ptr->m_pen = pen;
        emit d_ptr->updatedBars();
        emit penChanged();
    }
}

/*!
    Returns the pen used to draw the lines in the bar set.
*/
QPen QBarSet::pen() const
{
    if (d_ptr->m_pen == QChartPrivate::defaultPen())
        return QPen();
    else
        return d_ptr->m_pen;
}

/*!
    Sets the brush used to fill the bars in the bar set to \a brush.
*/
void QBarSet::setBrush(const QBrush &brush)
{
    if (d_ptr->m_brush != brush) {
        d_ptr->m_brush = brush;
        emit d_ptr->updatedBars();
        emit brushChanged();
    }
}

/*!
    Returns the brush used to fill the bars in the bar set.
*/
QBrush QBarSet::brush() const
{
    if (d_ptr->m_brush == QChartPrivate::defaultBrush())
        return QBrush();
    else
        return d_ptr->m_brush;
}

/*!
    Sets the brush used to draw values on top of this bar set to \a brush.
*/
void QBarSet::setLabelBrush(const QBrush &brush)
{
    if (d_ptr->m_labelBrush != brush) {
        d_ptr->m_labelBrush = brush;
        emit d_ptr->updatedBars();
        emit labelBrushChanged();
    }
}

/*!
    Returns the brush used to draw values on top of this bar set.
*/
QBrush QBarSet::labelBrush() const
{
    if (d_ptr->m_labelBrush == QChartPrivate::defaultBrush())
        return QBrush();
    else
        return d_ptr->m_labelBrush;
}

/*!
    Sets the font used to draw values on top of this bar set to \a font.
*/
void QBarSet::setLabelFont(const QFont &font)
{
    if (d_ptr->m_labelFont != font) {
        d_ptr->m_labelFont = font;
        emit d_ptr->updatedBars();
        emit labelFontChanged();
    }

}

/*!
    Returns the pen used to draw values on top of this bar set.
*/
QFont QBarSet::labelFont() const
{
    return d_ptr->m_labelFont;
}

/*!
    Returns the fill color for the bar set.
*/
QColor QBarSet::color()
{
    return brush().color();
}

/*!
    Sets the fill color for the bar set to \a color.
*/
void QBarSet::setColor(QColor color)
{
    QBrush b = brush();
    if ((b.color() != color) || (b.style() == Qt::NoBrush)) {
        b.setColor(color);
        if (b.style() == Qt::NoBrush) {
            // Set tyle to Qt::SolidPattern. (Default is Qt::NoBrush)
            // This prevents theme to override color defined in QML side:
            // BarSet { label: "Bob"; color:"red"; values: [1,2,3] }
            // The color must be obeyed, since user wanted it.
            b.setStyle(Qt::SolidPattern);
        }
        setBrush(b);
        emit colorChanged(color);
    }
}

/*!
    Returns the line color for the bar set.
*/
QColor QBarSet::borderColor()
{
    return pen().color();
}

/*!
    Sets the line color for the bar set to \a color.
*/
void QBarSet::setBorderColor(QColor color)
{
    QPen p = pen();
    if (p.color() != color) {
        p.setColor(color);
        setPen(p);
        emit borderColorChanged(color);
    }
}

/*!
    Returns the text color for the bar set.
*/
QColor QBarSet::labelColor()
{
    return labelBrush().color();
}

/*!
    Sets the text color for the bar set to \a color.
*/
void QBarSet::setLabelColor(QColor color)
{
    QBrush b = labelBrush();
    if (b == QBrush())
        b.setStyle(Qt::SolidPattern);

    if (d_ptr->m_labelBrush.color() != color) {
        b.setColor(color);
        setLabelBrush(b);
        emit labelColorChanged(color);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QBarSetPrivate::QBarSetPrivate(const QString label, QBarSet *parent) : QObject(parent),
    q_ptr(parent),
    m_label(label),
    m_pen(QChartPrivate::defaultPen()),
    m_brush(QChartPrivate::defaultBrush()),
    m_labelBrush(QChartPrivate::defaultBrush())
{
}

QBarSetPrivate::~QBarSetPrivate()
{
}

void QBarSetPrivate::append(QPointF value)
{
    if (isValidValue(value)) {
        m_values.append(value);
        emit restructuredBars();
    }
}

void QBarSetPrivate::append(QList<QPointF> values)
{
    for (int i = 0; i < values.count(); i++) {
        if (isValidValue(values.at(i)))
            m_values.append(values.at(i));
    }
    emit restructuredBars();
}

void QBarSetPrivate::append(QList<qreal> values)
{
    int index = m_values.count();
    for (int i = 0; i < values.count(); i++) {
        if (isValidValue(values.at(i))) {
            m_values.append(QPointF(index, values.at(i)));
            index++;
        }
    }
    emit restructuredBars();
}

void QBarSetPrivate::insert(const int index, const qreal value)
{
    m_values.insert(index, QPointF(index, value));
    emit restructuredBars();
}

void QBarSetPrivate::insert(const int index, const QPointF value)
{
    m_values.insert(index, value);
    emit restructuredBars();
}

int QBarSetPrivate::remove(const int index, const int count)
{
    int removeCount = count;

    if ((index < 0) || (m_values.count() == 0))
        return 0; // Invalid index or not values in list, remove nothing.
    else if ((index + count) > m_values.count())
        removeCount = m_values.count() - index; // Trying to remove more items than list has. Limit amount to be removed.

    int c = 0;
    while (c < removeCount) {
        m_values.removeAt(index);
        c++;
    }
    emit restructuredBars();
    return removeCount;
}

void QBarSetPrivate::replace(const int index, const qreal value)
{
    m_values.replace(index, QPointF(index, value));
    emit updatedLayout();
}

void QBarSetPrivate::replace(const int index, const QPointF value)
{
    m_values.replace(index, value);
    emit updatedLayout();
}

qreal QBarSetPrivate::pos(const int index)
{
    if (index < 0 || index >= m_values.count())
        return 0;
    return m_values.at(index).x();
}

qreal QBarSetPrivate::value(const int index)
{
    if (index < 0 || index >= m_values.count())
        return 0;
    return m_values.at(index).y();
}

#include "moc_qbarset.cpp"
#include "moc_qbarset_p.cpp"

QT_CHARTS_END_NAMESPACE
