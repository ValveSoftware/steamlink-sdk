/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

#include "qvalue3daxis_p.h"
#include "qvalue3daxisformatter_p.h"
#include "abstract3dcontroller_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QValue3DAxis
 * \inmodule QtDataVisualization
 * \brief The QValue3DAxis class is used for manipulating an axis of a graph.
 * \since QtDataVisualization 1.0
 *
 * QValue3DAxis provides an axis that can be given a range of values and segment and subsegment
 * counts to divide the range into.
 *
 * Labels are drawn between each segment. Grid lines are drawn between each segment and each
 * subsegment. \note If visible, there will always be at least two grid lines and labels indicating
 * the minimum and the maximum values of the range, as there is always at least one segment.
 */

/*!
 * \qmltype ValueAxis3D
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QValue3DAxis
 * \inherits AbstractAxis3D
 * \brief The ValueAxis3D type is used for manipulating an axis of a graph.
 *
 * This type provides an axis that can be given a range of values and segment and subsegment
 * counts to divide the range into.
 */


/*!
 * \qmlproperty int ValueAxis3D::segmentCount
 *
 * Defines the number of segments on the axis. This indicates how many labels are drawn. The number
 * of grid lines to be drawn is calculated with formula: \c {segments * subsegments + 1}.
 * The preset default is \c 5, and it can not be below \c 1.
 */

/*!
 * \qmlproperty int ValueAxis3D::subSegmentCount
 *
 * Defines the number of subsegments inside each segment on the axis. Grid lines are drawn between
 * each subsegment, in addition to each segment.
 * The preset default is \c 1, and it can not be below \c 1.
 */

/*!
 * \qmlproperty string ValueAxis3D::labelFormat
 *
 * Defines the label format to be used for the labels on this axis. How the format is interpreted
 * depends on the axis formatter and the locale in use. Using the default formatter and default
 * locale (\c{"C"}), the formatting uses QString::sprintf(). Supported specifiers are:
 * \c {d, i, o, x, X, f, F, e, E, g, G, c}. See QString::sprintf() for additional details.
 * For other locales, the default formatter uses reduced set of printf format specifiers:
 * \c {d, i, f, F, e, E, g, G}. In these cases, the only supported modifier is the precision
 * modifier for the floating point and exponential formats. The decimal point and other locale
 * dependent formatting is done according to the graph locale.
 *
 * \sa AbstractGraph3D::locale
 */

/*!
 * \qmlproperty ValueAxis3DFormatter ValueAxis3D::formatter
 * \since QtDataVisualization 1.1
 *
 * Defines the axis \a formatter to be used. Any existing formatter is deleted when a new formatter
 * is set.
 *
 */

/*!
 * \qmlproperty bool ValueAxis3D::reversed
 * \since QtDataVisualization 1.1
 *
 * If \c{true}, the axis will be rendered in reverse, i.e. the positions of minimum and maximum
 * values are swapped when the graph is rendered. This property doesn't affect the actual
 * minimum and maximum values of the axis.
 */

/*!
 * Constructs QValue3DAxis with the given \a parent.
 */
QValue3DAxis::QValue3DAxis(QObject *parent) :
    QAbstract3DAxis(new QValue3DAxisPrivate(this), parent)
{
    setFormatter(new QValue3DAxisFormatter);
}

/*!
 * Destroys QValue3DAxis.
 */
QValue3DAxis::~QValue3DAxis()
{
}

/*!
 * \property QValue3DAxis::segmentCount
 *
 * Defines the number of segments on the axis. This indicates how many labels are drawn. The number
 * of grid lines to be drawn is calculated with formula: \c {segments * subsegments + 1}.
 * The preset default is \c 5, and it can not be below \c 1.
 *
 * \sa setSubSegmentCount()
 */
void QValue3DAxis::setSegmentCount(int count)
{
    if (count <= 0) {
        qWarning() << "Warning: Illegal segment count automatically adjusted to a legal one:"
                   << count << "-> 1";
        count = 1;
    }
    if (dptr()->m_segmentCount != count) {
        dptr()->m_segmentCount = count;
        dptr()->emitLabelsChanged();
        emit segmentCountChanged(count);
    }
}

int QValue3DAxis::segmentCount() const
{
    return dptrc()->m_segmentCount;
}

/*!
 * \property QValue3DAxis::subSegmentCount
 *
 * Defines the number of subsegments inside each segment on the axis. Grid lines are drawn between
 * each subsegment, in addition to each segment.
 * The preset default is \c 1, and it can not be below \c 1.
 *
 * \sa setSegmentCount()
 */
void QValue3DAxis::setSubSegmentCount(int count)
{
    if (count <= 0) {
        qWarning() << "Warning: Illegal subsegment count automatically adjusted to a legal one:"
                   << count << "-> 1";
        count = 1;
    }
    if (dptr()->m_subSegmentCount != count) {
        dptr()->m_subSegmentCount = count;
        emit subSegmentCountChanged(count);
    }
}

int QValue3DAxis::subSegmentCount() const
{
    return dptrc()->m_subSegmentCount;
}

/*!
 * \property QValue3DAxis::labelFormat
 *
 * Defines the label format to be used for the labels on this axis. How the format is interpreted
 * depends on the axis formatter and the locale in use. Using the default formatter and default
 * locale (\c{"C"}), the formatting uses QString::sprintf(). Supported specifiers are:
 * \c {d, i, o, x, X, f, F, e, E, g, G, c}. See QString::sprintf() for additional details.
 * For other locales, the default formatter uses reduced set of printf format specifiers:
 * \c {d, i, f, F, e, E, g, G}. In these cases, the only supported modifier is the precision
 * modifier for the floating point and exponential formats. The decimal point and other locale
 * dependent formatting is done according to the graph locale.
 *
 * Usage example:
 *
 * \c {axis->setLabelFormat("%.2f mm");}
 *
 * \sa formatter, QAbstract3DGraph::locale
 */
void QValue3DAxis::setLabelFormat(const QString &format)
{
    if (dptr()->m_labelFormat != format) {
        dptr()->m_labelFormat = format;
        dptr()->emitLabelsChanged();
        emit labelFormatChanged(format);
    }
}

QString QValue3DAxis::labelFormat() const
{
    return dptrc()->m_labelFormat;
}

/*!
 * \property QValue3DAxis::formatter
 * \since QtDataVisualization 1.1
 *
 * Defines the axis \a formatter to be used. Any existing formatter is deleted when a new formatter
 * is set.
 */
void QValue3DAxis::setFormatter(QValue3DAxisFormatter *formatter)
{
    Q_ASSERT(formatter);

    if (formatter != dptr()->m_formatter) {
        delete dptr()->m_formatter;
        dptr()->m_formatter = formatter;
        formatter->setParent(this);
        formatter->d_ptr->setAxis(this);
        Abstract3DController *controller = qobject_cast<Abstract3DController *>(parent());
        if (controller)
            formatter->setLocale(controller->locale());
        emit formatterChanged(formatter);
        emit dptr()->formatterDirty();
    }
}

QValue3DAxisFormatter *QValue3DAxis::formatter() const
{
    return dptrc()->m_formatter;
}

/*!
 * \property QValue3DAxis::reversed
 * \since QtDataVisualization 1.1
 *
 * If \c{true}, the axis will be rendered in reverse, i.e. the positions of minimum and maximum
 * values are swapped when the graph is rendered. This property doesn't affect the actual
 * minimum and maximum values of the axis.
 */
void QValue3DAxis::setReversed(bool enable)
{
    if (dptr()->m_reversed != enable) {
        dptr()->m_reversed = enable;
        emit reversedChanged(enable);
    }
}

bool QValue3DAxis::reversed() const
{
    return dptrc()->m_reversed;
}

/*!
 * \internal
 */
QValue3DAxisPrivate *QValue3DAxis::dptr()
{
    return static_cast<QValue3DAxisPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QValue3DAxisPrivate *QValue3DAxis::dptrc() const
{
    return static_cast<const QValue3DAxisPrivate *>(d_ptr.data());
}

QValue3DAxisPrivate::QValue3DAxisPrivate(QValue3DAxis *q)
    : QAbstract3DAxisPrivate(q, QAbstract3DAxis::AxisTypeValue),
      m_segmentCount(5),
      m_subSegmentCount(1),
      m_labelFormat(Utils::defaultLabelFormat()),
      m_labelsDirty(true),
      m_formatter(0),
      m_reversed(false)
{
}

QValue3DAxisPrivate::~QValue3DAxisPrivate()
{
}

void QValue3DAxisPrivate::setRange(float min, float max, bool suppressWarnings)
{
    bool dirty = (min != m_min || max != m_max);

    QAbstract3DAxisPrivate::setRange(min, max, suppressWarnings);

    if (dirty)
        emitLabelsChanged();
}

void QValue3DAxisPrivate::setMin(float min)
{
    bool dirty = (min != m_min);

    QAbstract3DAxisPrivate::setMin(min);

    if (dirty)
        emitLabelsChanged();
}

void QValue3DAxisPrivate::setMax(float max)
{
    bool dirty = (max != m_max);

    QAbstract3DAxisPrivate::setMax(max);

    if (dirty)
        emitLabelsChanged();
}

void QValue3DAxisPrivate::emitLabelsChanged()
{
    m_labelsDirty = true;
    emit q_ptr->labelsChanged();
}

void QValue3DAxisPrivate::updateLabels()
{
    if (!m_labelsDirty)
        return;

    m_labelsDirty = false;

    m_formatter->d_ptr->recalculate();

    m_labels = m_formatter->labelStrings();
}

bool QValue3DAxisPrivate::allowZero()
{
    return m_formatter->allowZero();
}

bool QValue3DAxisPrivate::allowNegatives()
{
    return m_formatter->allowNegatives();
}

bool QValue3DAxisPrivate::allowMinMaxSame()
{
    return false;
}

QValue3DAxis *QValue3DAxisPrivate::qptr()
{
    return static_cast<QValue3DAxis *>(q_ptr);
}

QT_END_NAMESPACE_DATAVISUALIZATION
