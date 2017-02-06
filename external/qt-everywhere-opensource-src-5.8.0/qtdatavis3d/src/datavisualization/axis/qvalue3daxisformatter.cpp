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

#include "qvalue3daxisformatter_p.h"
#include "qvalue3daxis_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QValue3DAxisFormatter
 * \inmodule QtDataVisualization
 * \brief QValue3DAxisFormatter is base class for value axis formatters.
 * \since QtDataVisualization 1.1
 *
 * This class provides formatting rules for a linear QValue3DAxis. Subclass it if you
 * want to implement custom value axes.
 *
 * The base class has no public API beyond constructors and destructors. It is meant to be only
 * used internally. However, subclasses may implement public properties as needed.
 *
 * \sa QLogValue3DAxisFormatter
 */

/*!
 * \qmltype ValueAxis3DFormatter
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.1
 * \ingroup datavisualization_qml
 * \instantiates QValue3DAxisFormatter
 * \brief ValueAxis3DFormatter is base type for value axis formatters.
 *
 * This type provides formatting rules for a linear ValueAxis3D.
 * This type is the default type for ValueAxis3D and thus never needs to be explicitly created.
 * This type has not public functionality.
 */

/*!
 * \internal
 */
QValue3DAxisFormatter::QValue3DAxisFormatter(QValue3DAxisFormatterPrivate *d, QObject *parent) :
    QObject(parent),
    d_ptr(d)
{
}

/*!
 * Constructs a new QValue3DAxisFormatter instance with an optional \a parent.
 */
QValue3DAxisFormatter::QValue3DAxisFormatter(QObject *parent) :
    QObject(parent),
    d_ptr(new QValue3DAxisFormatterPrivate(this))
{
}

/*!
 * Destroys QValue3DAxisFormatter.
 */
QValue3DAxisFormatter::~QValue3DAxisFormatter()
{
}

/*!
 * Allow the parent axis to have negative values if \a allow is true.
 */
void QValue3DAxisFormatter::setAllowNegatives(bool allow)
{
    d_ptr->m_allowNegatives = allow;
}

/*!
 * \return \c true if negative values are valid values for parent axis.
 * The default implementation always returns true.
 */
bool QValue3DAxisFormatter::allowNegatives() const
{
    return d_ptr->m_allowNegatives;
}

/*!
 * Allow the parent axis to have zero value if \a allow is true.
 */
void QValue3DAxisFormatter::setAllowZero(bool allow)
{
    d_ptr->m_allowZero = allow;
}

/*!
 * \return \c true if zero is a valid value for parent axis.
 * The default implementation always returns true.
 */
bool QValue3DAxisFormatter::allowZero() const
{
    return d_ptr->m_allowZero;
}

/*!
 * Creates a new empty instance of this formatter. Must be reimplemented in a subclass.
 *
 * \return the new instance. The renderer uses this method to cache a copy of the
 * the formatter. The ownership of the new copy transfers to the caller.
 */
QValue3DAxisFormatter *QValue3DAxisFormatter::createNewInstance() const
{
    return new QValue3DAxisFormatter();
}

/*!
 * This method resizes and populates the label and grid line position arrays and the label strings
 * array, as well as calculates any values needed for mapping between value and position.
 * It is allowed to access the parent axis from inside this function.
 *
 * This method must be reimplemented in a subclass if the default array contents are not suitable.
 *
 * See gridPositions(), subGridPositions(), labelPositions(), and labelStrings() methods for
 * documentation about the arrays that need to be resized and populated.
 *
 * \sa gridPositions(), subGridPositions(), labelPositions(), labelStrings(), axis()
 */
void QValue3DAxisFormatter::recalculate()
{
    d_ptr->doRecalculate();
}

/*!
 * This method is used to format a string using the specified value and the specified format.
 * Reimplement this method in a subclass to resolve the formatted string for a given \a value
 * if the default formatting rules specified for QValue3DAxis::labelFormat property are not
 * sufficient.
 *
 * \return the formatted label string using a \a value and a \a format.
 *
 * \sa recalculate(), labelStrings(), QValue3DAxis::labelFormat
 */
QString QValue3DAxisFormatter::stringForValue(qreal value, const QString &format) const
{
    return d_ptr->stringForValue(value, format);
}

/*!
 * Reimplement this method if the position cannot be resolved by linear interpolation
 * between the parent axis minimum and maximum values.
 *
 * \return the normalized position along the axis for the given \a value.
 * The returned value should be between 0.0 (for minimum value) and 1.0 (for maximum value),
 * inclusive, if the value is within the parent axis range.
 *
 * \sa recalculate(), valueAt()
 */
float QValue3DAxisFormatter::positionAt(float value) const
{
    return d_ptr->positionAt(value);
}

/*!
 * Reimplement this method if the value cannot be resolved by linear interpolation
 * between the parent axis minimum and maximum values.
 *
 * \return the value at the normalized \a position along the axis.
 * The \a position value should be between 0.0 (for minimum value) and 1.0 (for maximum value),
 * inclusive to obtain values within the parent axis range.
 *
 * \sa recalculate(), positionAt()
 */
float QValue3DAxisFormatter::valueAt(float position) const
{
    return d_ptr->valueAt(position);
}

/*!
 * Copies all necessary values for resolving positions, values, and strings with this formatter
 * from this formatter to the \a copy.
 * When reimplementing this method in a subclass, call the the superclass version at some point.
 * The renderer uses this method to cache a copy of the the formatter.
 *
 * \return the new copy. The ownership of the new copy transfers to the caller.
 */
void QValue3DAxisFormatter::populateCopy(QValue3DAxisFormatter &copy) const
{
    d_ptr->doPopulateCopy(*(copy.d_ptr.data()));
}

/*!
 * Marks this formatter dirty, prompting the renderer to make a new copy of its cache on the next
 * renderer synchronization. This method should be called by a subclass whenever the formatter
 * is changed in a way that affects the resolved values. Specify \c true for \a labelsChange
 * parameter if the change was such that it requires regenerating the parent axis label strings.
 */
void QValue3DAxisFormatter::markDirty(bool labelsChange)
{
    d_ptr->markDirty(labelsChange);
}

/*!
 * \return the parent axis. The parent axis must only be accessed in recalculate()
 * method to maintain thread safety in environments using a threaded renderer.
 *
 * \sa recalculate()
 */
QValue3DAxis *QValue3DAxisFormatter::axis() const
{
    return d_ptr->m_axis;
}

/*!
 * \return a reference to the array of normalized grid line positions.
 * The default array size is equal to the segment count of the parent axis plus one, but
 * a subclassed implementation of recalculate method may resize the array differently.
 * The values should be between 0.0 (for minimum value) and 1.0 (for maximum value), inclusive.
 *
 * \sa QValue3DAxis::segmentCount, recalculate()
 */
QVector<float> &QValue3DAxisFormatter::gridPositions() const
{
    return d_ptr->m_gridPositions;
}

/*!
 * \return a reference to the array of normalized subgrid line positions.
 * The default array size is equal to segment count of the parent axis times sub-segment count
 * of the parent axis minus one, but a subclassed implementation of recalculate method may resize
 * the array differently.
 * The values should be between 0.0 (for minimum value) and 1.0 (for maximum value), inclusive.
 *
 * \sa QValue3DAxis::segmentCount, QValue3DAxis::subSegmentCount, recalculate()
 */
QVector<float> &QValue3DAxisFormatter::subGridPositions() const
{
    return d_ptr->m_subGridPositions;
}

/*!
 * \return a reference to the array of normalized label positions.
 * The default array size is equal to the segment count of the parent axis plus one, but
 * a subclassed implementation of recalculate method may resize the array differently.
 * The values should be between 0.0 (for minimum value) and 1.0 (for maximum value), inclusive.
 * The default behavior is that the label at the index zero corresponds to the minimum value
 * of the axis.
 *
 * \sa QValue3DAxis::segmentCount, QAbstract3DAxis::labels, recalculate()
 */
QVector<float> &QValue3DAxisFormatter::labelPositions() const
{
    return d_ptr->m_labelPositions;
}

/*!
 * \return a reference to the string list containing formatter label strings.
 * The array size must be equal to the size of the label positions array and
 * the indexes correspond to that array as well.
 *
 * \sa labelPositions()
 */
QStringList &QValue3DAxisFormatter::labelStrings() const
{
    return d_ptr->m_labelStrings;
}

/*!
 * Sets the \a locale that this formatter uses.
 * The graph automatically sets the formatter's locale to a graph's locale whenever the parent axis
 * is set as an active axis of the graph, the axis formatter is set to an axis attached to
 * the graph, or the graph's locale changes.
 *
 * \sa locale(), QAbstract3DGraph::locale
 */
void QValue3DAxisFormatter::setLocale(const QLocale &locale)
{
    d_ptr->m_cLocaleInUse = (locale == QLocale::c());
    d_ptr->m_locale = locale;
    markDirty(true);
}
/*!
 * \return the current locale this formatter is using.
 */
QLocale QValue3DAxisFormatter::locale() const
{
    return d_ptr->m_locale;
}

// QValue3DAxisFormatterPrivate
QValue3DAxisFormatterPrivate::QValue3DAxisFormatterPrivate(QValue3DAxisFormatter *q)
    : QObject(0),
      q_ptr(q),
      m_needsRecalculate(true),
      m_min(0.0f),
      m_max(0.0f),
      m_rangeNormalizer(0.0f),
      m_axis(0),
      m_preparsedParamType(Utils::ParamTypeUnknown),
      m_allowNegatives(true),
      m_allowZero(true),
      m_formatPrecision(6), // 6 and 'g' are defaults in Qt API for format precision and spec
      m_formatSpec('g'),
      m_cLocaleInUse(true)
{
}

QValue3DAxisFormatterPrivate::~QValue3DAxisFormatterPrivate()
{
}

void QValue3DAxisFormatterPrivate::recalculate()
{
    // Only recalculate if we need to and have m_axis pointer. If we do not have
    // m_axis, either we are not attached to an axis or this is a renderer cache.
    if (m_axis && m_needsRecalculate) {
        m_min = m_axis->min();
        m_max = m_axis->max();
        m_rangeNormalizer = (m_max - m_min);

        q_ptr->recalculate();
        m_needsRecalculate = false;
    }
}

void QValue3DAxisFormatterPrivate::doRecalculate()
{
    int segmentCount = m_axis->segmentCount();
    int subGridCount = m_axis->subSegmentCount() - 1;
    QString labelFormat =  m_axis->labelFormat();

    m_gridPositions.resize(segmentCount + 1);
    m_subGridPositions.resize(segmentCount * subGridCount);

    m_labelPositions.resize(segmentCount + 1);
    m_labelStrings.clear();
    m_labelStrings.reserve(segmentCount + 1);

    // Use qreals for intermediate calculations for better accuracy on label values
    qreal segmentStep = 1.0 / qreal(segmentCount);
    qreal subSegmentStep = 0;
    if (subGridCount > 0)
        subSegmentStep = segmentStep / qreal(subGridCount + 1);

    // Calculate positions
    qreal rangeNormalizer = qreal(m_max - m_min);
    for (int i = 0; i < segmentCount; i++) {
        qreal gridValue = segmentStep * qreal(i);
        m_gridPositions[i] = float(gridValue);
        m_labelPositions[i] = float(gridValue);
        m_labelStrings << q_ptr->stringForValue(gridValue * rangeNormalizer + qreal(m_min),
                                                labelFormat);
        if (m_subGridPositions.size()) {
            for (int j = 0; j < subGridCount; j++)
                m_subGridPositions[i * subGridCount + j] = gridValue + subSegmentStep * (j + 1);
        }
    }

    // Ensure max value doesn't suffer from any rounding errors
    m_gridPositions[segmentCount] = 1.0f;
    m_labelPositions[segmentCount] = 1.0f;
    m_labelStrings << q_ptr->stringForValue(qreal(m_max), labelFormat);
}

void QValue3DAxisFormatterPrivate::populateCopy(QValue3DAxisFormatter &copy)
{
    recalculate();
    q_ptr->populateCopy(copy);
}

void QValue3DAxisFormatterPrivate::doPopulateCopy(QValue3DAxisFormatterPrivate &copy)
{
    copy.m_min = m_min;
    copy.m_max = m_max;
    copy.m_rangeNormalizer = m_rangeNormalizer;

    copy.m_gridPositions = m_gridPositions;
    copy.m_labelPositions = m_labelPositions;
    copy.m_subGridPositions = m_subGridPositions;
}

QString QValue3DAxisFormatterPrivate::stringForValue(qreal value, const QString &format)
{
    if (m_previousLabelFormat.compare(format)) {
        // Format string different than the previous one used, reparse it
        m_labelFormatArray = format.toUtf8();
        m_previousLabelFormat = format;
        m_preparsedParamType = Utils::preParseFormat(format, m_formatPreStr, m_formatPostStr,
                                                     m_formatPrecision, m_formatSpec);
    }

    if (m_cLocaleInUse) {
        return Utils::formatLabelSprintf(m_labelFormatArray, m_preparsedParamType, value);
    } else {
        return Utils::formatLabelLocalized(m_preparsedParamType, value, m_locale, m_formatPreStr,
                                           m_formatPostStr, m_formatPrecision, m_formatSpec,
                                           m_labelFormatArray);
    }
}

float QValue3DAxisFormatterPrivate::positionAt(float value) const
{
    return ((value - m_min) / m_rangeNormalizer);
}

float QValue3DAxisFormatterPrivate::valueAt(float position) const
{
    return ((position * m_rangeNormalizer) + m_min);
}

void QValue3DAxisFormatterPrivate::setAxis(QValue3DAxis *axis)
{
    Q_ASSERT(axis);

    // These signals are all connected to markDirtyNoLabelChange slot, even though most of them
    // do require labels to be regenerated. This is because the label regeneration is triggered
    // elsewhere in these cases.
    connect(axis, &QValue3DAxis::segmentCountChanged,
            this, &QValue3DAxisFormatterPrivate::markDirtyNoLabelChange);
    connect(axis, &QValue3DAxis::subSegmentCountChanged,
            this, &QValue3DAxisFormatterPrivate::markDirtyNoLabelChange);
    connect(axis, &QValue3DAxis::labelFormatChanged,
            this, &QValue3DAxisFormatterPrivate::markDirtyNoLabelChange);
    connect(axis, &QAbstract3DAxis::rangeChanged,
            this, &QValue3DAxisFormatterPrivate::markDirtyNoLabelChange);

    m_axis = axis;
}

void QValue3DAxisFormatterPrivate::markDirty(bool labelsChange)
{
    m_needsRecalculate = true;
    if (m_axis) {
        if (labelsChange)
            m_axis->dptr()->emitLabelsChanged();
        if (m_axis && m_axis->orientation() != QAbstract3DAxis::AxisOrientationNone)
            emit m_axis->dptr()->formatterDirty();
    }
}

void QValue3DAxisFormatterPrivate::markDirtyNoLabelChange()
{
    markDirty(false);
}

QT_END_NAMESPACE_DATAVISUALIZATION
