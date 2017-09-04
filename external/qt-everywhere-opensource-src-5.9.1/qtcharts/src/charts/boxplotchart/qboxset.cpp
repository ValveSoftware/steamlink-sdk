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

#include <QtCharts/QBoxSet>
#include <private/qboxset_p.h>
#include <private/charthelpers_p.h>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QBoxSet
    \inmodule Qt Charts
    \brief The QBoxSet class represents one item in a box-and-whiskers chart.

    A box-and-whiskers item is a graphical representation of a range and three median values
    that is constructed from five different values. There are two ways to specify the values.
    The first one is by using a constructor or stream operator (<<). The values have to be
    specified in the following order: lower extreme, lower quartile, median, upper quartile,
    and upper extreme.

    The second way is to create an empty QBoxSet instance and specify the values using the
    setValue() method.

    See the \l{Box and Whiskers Example}{box-and-whiskers chart example} to learn how to
    create a box-and-whiskers chart.

    \sa QBoxPlotSeries
*/
/*!
    \enum QBoxSet::ValuePositions

    This enum type defines the values of a box-and-whiskers item:

    \value LowerExtreme The smallest value of the box-and-whiskers item.
    \value LowerQuartile The median value of the lower half of the box-and-whiskers item.
    \value Median The median value of the box-and-whiskers item.
    \value UpperQuartile The median value of the upper half of the box-and-whiskers item.
    \value UpperExtreme The largest value of the box-and-whiskers item.
*/
/*!
    \property QBoxSet::pen
    \brief The pen used to draw the lines of the box-and-whiskers item.
*/
/*!
    \property QBoxSet::brush
    \brief The brush used fill the box of the box-and-whiskers item.
*/

/*!
    \qmlproperty QString BoxSet::brushFilename
    The name of the file used as a brush for the box-and-whiskers item.
*/

/*!
    \fn void QBoxSet::clicked()
    This signal is emitted when the user clicks a box-and-whiskers item in the chart.
*/

/*!
    \fn void QBoxSet::pressed()
    This signal is emitted when the user clicks a box-and-whiskers item in the chart
    and holds down the mouse button.
*/

/*!
    \fn void QBoxSet::released()
    This signal is emitted when the user releases the mouse press on a box-and-whiskers item.
*/

/*!
    \fn void QBoxSet::doubleClicked()
    This signal is emitted when the user double-clicks a box-and-whiskers item.
*/

/*!
    \fn void QBoxSet::hovered(bool status)

    This signal is emitted when a mouse is hovered over a box-and-whiskers item in a chart.
    When the mouse moves over the item, \a status turns \c true, and when the mouse moves
    away again, it turns \c false.
*/
/*!
    \fn void QBoxSet::penChanged()
    This signal is emitted when the pen of the box-and-whiskers item changes.
    \sa pen
*/
/*!
    \fn void QBoxSet::brushChanged()
    This signal is emitted when the brush of the box-and-whiskers item changes.
    \sa brush
*/
/*!
    \fn void QBoxSet::valuesChanged()
    This signal is emitted when multiple values of the box-and-whiskers item change.
    \sa append()
*/
/*!
    \fn void QBoxSet::valueChanged(int index)
    This signal is emitted when the value of the box-and-whiskers item specified by
    \a index is modified.
    \sa at()
*/
/*!
    \fn void QBoxSet::cleared()
    This signal is emitted when all the values of the box-and-whiskers item are set to 0.
*/

/*!
    Constructs a box-and-whiskers item with the optional label \a label and parent \a parent.
*/
QBoxSet::QBoxSet(const QString label, QObject *parent)
    : QObject(parent),
      d_ptr(new QBoxSetPrivate(label, this))
{
}

/*!
    Constructs a box-and-whiskers item with the following ordered values: \a le specifies the
    lower extreme, \a lq the lower quartile, \a m the median, \a uq the upper quartile, and
    \a ue the upper quartile. Optionally, the \a label and \a parent can be specified.
 */
QBoxSet::QBoxSet(const qreal le, const qreal lq, const qreal m, const qreal uq, const qreal ue, const QString label, QObject *parent)
    : QObject(parent),
      d_ptr(new QBoxSetPrivate(label, this))
{
    d_ptr->append(le);
    d_ptr->append(lq);
    d_ptr->append(m);
    d_ptr->append(uq);
    d_ptr->append(ue);
}

/*!
    Destroys the a box-and-whiskers item.
*/
QBoxSet::~QBoxSet()
{
}

/*!
    Appends the new value specified by \a value to the end of the box-and-whiskers item.
*/
void QBoxSet::append(const qreal value)
{
    if (d_ptr->append(value))
        emit valueChanged(d_ptr->m_appendCount - 1);
}

/*!
    Appends a list of real values specified by \a values to the end of the box-and-whiskers item.
    \sa append()
*/
void QBoxSet::append(const QList<qreal> &values)
{
    if (d_ptr->append(values))
        emit valuesChanged();
}

/*!
    Sets the label specified by \a label for the category of the box-and-whiskers item.
*/
void QBoxSet::setLabel(const QString label)
{
    d_ptr->m_label = label;
}

/*!
    Returns the label of the category of the box-and-whiskers item.
*/
QString QBoxSet::label() const
{
    return d_ptr->m_label;
}

/*!
    A convenience operator for appending the real value specified by \a value to the end
    of the box-and-whiskers item.
    \sa append()
*/
QBoxSet &QBoxSet::operator << (const qreal &value)
{
    append(value);
    return *this;
}

/*!
    Sets the value specified by \a value in the position specified by \a index.
    The index can be specified by using the ValuePositions enumeration values.
*/
void QBoxSet::setValue(const int index, const qreal value)
{
    d_ptr->setValue(index, value);
    emit valueChanged(index);
}

/*!
    Sets all the values of the box-and-whiskers item to 0.
 */
void QBoxSet::clear()
{
    d_ptr->clear();
    emit cleared();
}

/*!
    Returns the value of the box-and-whiskers item specified by \a index.
    The index can be specified by using ValuePositions enumeration values.
    If the index is out of bounds, 0.0 is returned.
*/
qreal QBoxSet::at(const int index) const
{
    if (index < 0 || index >= 5)
        return 0;
    return d_ptr->m_values[index];
}

/*!
    Returns the value of the box-and-whiskers item specified by \a index.
    The index can be specified by using ValuePositions enumeration values.
    If the index is out of bounds, 0.0 is returned.
*/
qreal QBoxSet::operator [](const int index) const
{
    return at(index);
}

/*!
    Returns the number of values appended to the box-and-whiskers item.
*/
int QBoxSet::count() const
{
    return d_ptr->m_appendCount;
}

/*!
    Sets the pen used to draw the box-and-whiskers item to \a pen.
*/
void QBoxSet::setPen(const QPen &pen)
{
    if (d_ptr->m_pen != pen) {
        d_ptr->m_pen = pen;
        emit d_ptr->updatedBox();
        emit penChanged();
    }
}

/*!
    Returns the pen used to draw the box-and-whiskers item.
*/
QPen QBoxSet::pen() const
{
    return d_ptr->m_pen;
}

/*!
    Sets the brush used to fill the box-and-whiskers item to \a brush.
*/
void QBoxSet::setBrush(const QBrush &brush)
{
    if (d_ptr->m_brush != brush) {
        d_ptr->m_brush = brush;
        emit d_ptr->updatedBox();
        emit brushChanged();
    }
}

/*!
    Returns the brush used to fill the box-and-whiskers item.
*/
QBrush QBoxSet::brush() const
{
    return d_ptr->m_brush;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QBoxSetPrivate::QBoxSetPrivate(const QString label, QBoxSet *parent) : QObject(parent),
    q_ptr(parent),
    m_label(label),
    m_valuesCount(5),
    m_appendCount(0),
    m_pen(QPen(Qt::NoPen)),
    m_brush(QBrush(Qt::NoBrush)),
    m_series(0)
{
    m_values = new qreal[m_valuesCount];
}

QBoxSetPrivate::~QBoxSetPrivate()
{
    delete[] m_values;
}

bool QBoxSetPrivate::append(qreal value)
{
    if (isValidValue(value) && m_appendCount < m_valuesCount) {
        m_values[m_appendCount++] = value;
        emit restructuredBox();

        return true;
    }
    return false;
}

bool QBoxSetPrivate::append(QList<qreal> values)
{
    bool success = false;

    for (int i = 0; i < values.count(); i++) {
        if (isValidValue(values.at(i)) && m_appendCount < m_valuesCount) {
            success = true;
            m_values[m_appendCount++] = values.at(i);
        }
    }

    if (success)
        emit restructuredBox();

    return success;
}

void QBoxSetPrivate::clear()
{
    m_appendCount = 0;
    for (int i = 0; i < m_valuesCount; i++)
         m_values[i] = 0.0;
    emit restructuredBox();
}

void QBoxSetPrivate::setValue(const int index, const qreal value)
{
    if (index < m_valuesCount) {
        m_values[index] = value;
        emit updatedLayout();
    }
}

qreal QBoxSetPrivate::value(const int index)
{
    if (index < 0 || index >= m_valuesCount)
        return 0;
    return m_values[index];
}

#include "moc_qboxset.cpp"
#include "moc_qboxset_p.cpp"

QT_CHARTS_END_NAMESPACE
