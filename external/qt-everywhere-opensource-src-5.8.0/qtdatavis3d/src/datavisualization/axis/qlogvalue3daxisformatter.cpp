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

#include "qlogvalue3daxisformatter_p.h"
#include "qvalue3daxis_p.h"
#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QLogValue3DAxisFormatter
 * \inmodule QtDataVisualization
 * \brief QLogValue3DAxisFormatter implements logarithmic value axis formatter.
 * \since QtDataVisualization 1.1
 *
 * This class provides formatting rules for a logarithmic QValue3DAxis.
 *
 * When a QLogValue3DAxisFormatter is attached to a QValue3DAxis, the axis range
 * cannot include negative values or the zero.
 *
 * \sa QValue3DAxisFormatter
 */

/*!
 * \qmltype LogValueAxis3DFormatter
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.1
 * \ingroup datavisualization_qml
 * \instantiates QLogValue3DAxisFormatter
 * \inherits ValueAxis3DFormatter
 * \brief LogValueAxis3DFormatter implements logarithmic value axis formatter.
 *
 * This type provides formatting rules for a logarithmic ValueAxis3D.
 * When a LogValueAxis3DFormatter is attached to a ValueAxis3D, the axis range
 * cannot include negative values or the zero.
 */

/*!
 * \qmlproperty real LogValueAxis3DFormatter::base
 *
 * The \a base of the logarithm used to map axis values. If the base is non-zero, the parent axis
 * segment count will be ignored when the grid line and label positions are calculated.
 * If you want the range to be divided into equal segments like normal value axis, set this
 * property value to zero.
 *
 * The \a base has to be zero or positive value and not equal to one.
 * Defaults to ten.
 *
 * \sa ValueAxis3D::segmentCount
 */

/*!
 * \qmlproperty bool LogValueAxis3DFormatter::autoSubGrid
 *
 * If this property value is set to \c true, the parent axis sub-segment count is ignored
 * when calculating sub-grid line positions. The sub-grid positions are generated automatically
 * according to the base property value.
 * The number of sub-grid lines is set to base value minus one, rounded down.
 * This property is ignored when base property value is zero.
 * Defaults to \c true.
 *
 * \sa base, ValueAxis3D::subSegmentCount
 */

/*!
 * \qmlproperty bool LogValueAxis3DFormatter::showEdgeLabels
 *
 * When the base property value is non-zero, the whole axis range is often not equally divided into
 * segments. The first and last segments are often smaller than the other segments.
 * In extreme cases this can lead to overlapping labels on the first and last two grid lines.
 * By setting this property to \c false, you can suppress showing the minimum and maximum labels
 * for the axis in cases where the segments do not exactly fit the axis.
 * Defaults to \c true.
 *
 * \sa base, AbstractAxis3D::labels
 */

/*!
 * \internal
 */
QLogValue3DAxisFormatter::QLogValue3DAxisFormatter(QLogValue3DAxisFormatterPrivate *d,
                                                   QObject *parent) :
    QValue3DAxisFormatter(d, parent)
{
    setAllowNegatives(false);
    setAllowZero(false);
}

/*!
 * Constructs a new QLogValue3DAxisFormatter instance with optional \a parent.
 */
QLogValue3DAxisFormatter::QLogValue3DAxisFormatter(QObject *parent) :
    QValue3DAxisFormatter(new QLogValue3DAxisFormatterPrivate(this), parent)
{
    setAllowNegatives(false);
    setAllowZero(false);
}

/*!
 * Destroys QLogValue3DAxisFormatter.
 */
QLogValue3DAxisFormatter::~QLogValue3DAxisFormatter()
{
}

/*!
 * \property QLogValue3DAxisFormatter::base
 *
 * The \a base of the logarithm used to map axis values. If the base is non-zero, the parent axis
 * segment count will be ignored when the grid line and label positions are calculated.
 * If you want the range to be divided into equal segments like normal value axis, set this
 * property value to zero.
 *
 * The \a base has to be zero or positive value and not equal to one.
 * Defaults to ten.
 *
 * \sa QValue3DAxis::segmentCount
 */
void QLogValue3DAxisFormatter::setBase(qreal base)
{
    if (base < 0.0f || base == 1.0f) {
        qWarning() << "Warning: The logarithm base must be greater than 0 and not equal to 1,"
                   << "attempted:" << base;
        return;
    }
    if (dptr()->m_base != base) {
        dptr()->m_base = base;
        markDirty(true);
        emit baseChanged(base);
    }
}

qreal QLogValue3DAxisFormatter::base() const
{
    return dptrc()->m_base;
}

/*!
 * \property QLogValue3DAxisFormatter::autoSubGrid
 *
 * If this property value is set to \c true, the parent axis sub-segment count is ignored
 * when calculating sub-grid line positions. The sub-grid positions are generated automatically
 * according to the base property value.
 * The number of sub-grid lines is set to base value minus one, rounded down.
 * This property is ignored when base property value is zero.
 * Defaults to \c true.
 *
 * \sa base, QValue3DAxis::subSegmentCount
 */
void QLogValue3DAxisFormatter::setAutoSubGrid(bool enabled)
{
    if (dptr()->m_autoSubGrid != enabled) {
        dptr()->m_autoSubGrid = enabled;
        markDirty(false);
        emit autoSubGridChanged(enabled);
    }
}

bool QLogValue3DAxisFormatter::autoSubGrid() const
{
    return dptrc()->m_autoSubGrid;
}

/*!
 * \property QLogValue3DAxisFormatter::showEdgeLabels
 *
 * When the base property value is non-zero, the whole axis range is often not equally divided into
 * segments. The first and last segments are often smaller than the other segments.
 * In extreme cases this can lead to overlapping labels on the first and last two grid lines.
 * By setting this property to \c false, you can suppress showing the minimum and maximum labels
 * for the axis in cases where the segments do not exactly fit the axis.
 * Defaults to \c true.
 *
 * \sa base, QAbstract3DAxis::labels
 */
void QLogValue3DAxisFormatter::setShowEdgeLabels(bool enabled)
{
    if (dptr()->m_showEdgeLabels != enabled) {
        dptr()->m_showEdgeLabels = enabled;
        markDirty(true);
        emit showEdgeLabelsChanged(enabled);
    }
}

bool QLogValue3DAxisFormatter::showEdgeLabels() const
{
    return dptrc()->m_showEdgeLabels;
}

/*!
 * \internal
 */
QValue3DAxisFormatter *QLogValue3DAxisFormatter::createNewInstance() const
{
    return new QLogValue3DAxisFormatter();
}

/*!
 * \internal
 */
void QLogValue3DAxisFormatter::recalculate()
{
    dptr()->recalculate();
}

/*!
 * \internal
 */
float QLogValue3DAxisFormatter::positionAt(float value) const
{
    return dptrc()->positionAt(value);
}

/*!
 * \internal
 */
float QLogValue3DAxisFormatter::valueAt(float position) const
{
    return dptrc()->valueAt(position);
}

/*!
 * \internal
 */
void QLogValue3DAxisFormatter::populateCopy(QValue3DAxisFormatter &copy) const
{
    QValue3DAxisFormatter::populateCopy(copy);
    dptrc()->populateCopy(copy);
}

/*!
 * \internal
 */
QLogValue3DAxisFormatterPrivate *QLogValue3DAxisFormatter::dptr()
{
    return static_cast<QLogValue3DAxisFormatterPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QLogValue3DAxisFormatterPrivate *QLogValue3DAxisFormatter::dptrc() const
{
    return static_cast<const QLogValue3DAxisFormatterPrivate *>(d_ptr.data());
}

// QLogValue3DAxisFormatterPrivate
QLogValue3DAxisFormatterPrivate::QLogValue3DAxisFormatterPrivate(QLogValue3DAxisFormatter *q)
    : QValue3DAxisFormatterPrivate(q),
      m_base(10.0),
      m_logMin(0.0),
      m_logMax(0.0),
      m_logRangeNormalizer(0.0),
      m_autoSubGrid(true),
      m_showEdgeLabels(true),
      m_evenMinSegment(true),
      m_evenMaxSegment(true)
{
}

QLogValue3DAxisFormatterPrivate::~QLogValue3DAxisFormatterPrivate()
{
}

void QLogValue3DAxisFormatterPrivate::recalculate()
{
    // When doing position/value mappings, base doesn't matter, so just use natural logarithm
    m_logMin = qLn(qreal(m_min));
    m_logMax = qLn(qreal(m_max));
    m_logRangeNormalizer = m_logMax - m_logMin;

    int subGridCount = m_axis->subSegmentCount() - 1;
    int segmentCount = m_axis->segmentCount();
    QString labelFormat =  m_axis->labelFormat();
    qreal segmentStep;
    if (m_base > 0.0) {
        // Update parent axis segment counts
        qreal logMin = qLn(qreal(m_min)) / qLn(m_base);
        qreal logMax = qLn(qreal(m_max)) / qLn(m_base);
        qreal logRangeNormalizer = logMax - logMin;

        qreal minDiff = qCeil(logMin) - logMin;
        qreal maxDiff = logMax - qFloor(logMax);

        m_evenMinSegment = qFuzzyCompare(0.0, minDiff);
        m_evenMaxSegment = qFuzzyCompare(0.0, maxDiff);

        segmentCount = qRound(logRangeNormalizer - minDiff - maxDiff);

        if (!m_evenMinSegment)
            segmentCount++;
        if (!m_evenMaxSegment)
            segmentCount++;

        segmentStep = 1.0 / logRangeNormalizer;

        if (m_autoSubGrid) {
            subGridCount = qCeil(m_base) - 2; // -2 for subgrid because subsegment count is base - 1
            if (subGridCount < 0)
                subGridCount = 0;
        }

        m_gridPositions.resize(segmentCount + 1);
        m_subGridPositions.resize(segmentCount * subGridCount);
        m_labelPositions.resize(segmentCount + 1);
        m_labelStrings.clear();
        m_labelStrings.reserve(segmentCount + 1);

        // Calculate segment positions
        int index = 0;
        if (!m_evenMinSegment) {
            m_gridPositions[0] = 0.0f;
            m_labelPositions[0] = 0.0f;
            if (m_showEdgeLabels)
                m_labelStrings << qptr()->stringForValue(qreal(m_min), labelFormat);
            else
                m_labelStrings << QString();
            index++;
        }
        for (int i = 0; i < segmentCount; i++) {
            float gridValue = float((minDiff + qreal(i)) / qreal(logRangeNormalizer));
            m_gridPositions[index] = gridValue;
            m_labelPositions[index] = gridValue;
            m_labelStrings << qptr()->stringForValue(qPow(m_base, minDiff + qreal(i) + logMin),
                                                    labelFormat);
            index++;
        }
        // Ensure max value doesn't suffer from any rounding errors
        m_gridPositions[segmentCount] = 1.0f;
        m_labelPositions[segmentCount] = 1.0f;
        QString finalLabel;
        if (m_showEdgeLabels || m_evenMaxSegment)
            finalLabel = qptr()->stringForValue(qreal(m_max), labelFormat);

        if (m_labelStrings.size() > segmentCount)
            m_labelStrings.replace(segmentCount, finalLabel);
        else
            m_labelStrings << finalLabel;
    } else {
        // Grid lines and label positions are the same as the parent class, so call parent impl
        // first to populate those
        QValue3DAxisFormatterPrivate::doRecalculate();

        // Label string list needs to be repopulated
        segmentStep = 1.0 / qreal(segmentCount);

        m_labelStrings << qptr()->stringForValue(qreal(m_min), labelFormat);
        for (int i = 1; i < m_labelPositions.size() - 1; i++)
            m_labelStrings[i] = qptr()->stringForValue(qExp(segmentStep * qreal(i)
                                                           * m_logRangeNormalizer + m_logMin),
                                                      labelFormat);
        m_labelStrings << qptr()->stringForValue(qreal(m_max), labelFormat);

        m_evenMaxSegment = true;
        m_evenMinSegment = true;
    }

    // Subgrid line positions are logarithmically spaced
    if (subGridCount > 0) {
        float oneSegmentRange = valueAt(float(segmentStep)) - m_min;
        float subSegmentStep = oneSegmentRange / float(subGridCount + 1);

        // Since the logarithm has the same curvature across whole axis range, we can just calculate
        // subgrid positions for the first segment and replicate them to other segments.
        QVector<float> actualSubSegmentSteps(subGridCount);

        for (int i = 0; i < subGridCount; i++) {
            float currentSubPosition = positionAt(m_min + ((i + 1) * subSegmentStep));
            actualSubSegmentSteps[i] = currentSubPosition;
        }

        float firstPartialSegmentAdjustment = float(segmentStep) - m_gridPositions.at(1);
        for (int i = 0; i < segmentCount; i++) {
            for (int j = 0; j < subGridCount; j++) {
                float position = m_gridPositions.at(i) + actualSubSegmentSteps.at(j);
                if (!m_evenMinSegment && i == 0)
                    position -= firstPartialSegmentAdjustment;
                if (position > 1.0f)
                    position = 1.0f;
                if (position < 0.0f)
                    position = 0.0f;
                m_subGridPositions[i * subGridCount + j] = position;
            }
        }
    }
}

void QLogValue3DAxisFormatterPrivate::populateCopy(QValue3DAxisFormatter &copy) const
{
    QLogValue3DAxisFormatter *logFormatter = static_cast<QLogValue3DAxisFormatter *>(&copy);
    QLogValue3DAxisFormatterPrivate *priv = logFormatter->dptr();

    priv->m_base = m_base;
    priv->m_logMin = m_logMin;
    priv->m_logMax = m_logMax;
    priv->m_logRangeNormalizer = m_logRangeNormalizer;
}

float QLogValue3DAxisFormatterPrivate::positionAt(float value) const
{
    qreal logValue = qLn(qreal(value));
    float retval = float((logValue - m_logMin) / m_logRangeNormalizer);

    return retval;
}

float QLogValue3DAxisFormatterPrivate::valueAt(float position) const
{
    qreal logValue = (qreal(position) * m_logRangeNormalizer) + m_logMin;
    return float(qExp(logValue));
}

QLogValue3DAxisFormatter *QLogValue3DAxisFormatterPrivate::qptr()
{
    return static_cast<QLogValue3DAxisFormatter *>(q_ptr);
}

QT_END_NAMESPACE_DATAVISUALIZATION
