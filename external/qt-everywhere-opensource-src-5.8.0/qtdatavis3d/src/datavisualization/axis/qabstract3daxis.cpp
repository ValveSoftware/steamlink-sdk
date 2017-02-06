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

#include "qabstract3daxis_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QAbstract3DAxis
 * \inmodule QtDataVisualization
 * \brief QAbstract3DAxis is base class for axes of a graph.
 * \since QtDataVisualization 1.0
 *
 * You should not need to use this class directly, but one of its subclasses instead.
 *
 * \sa QCategory3DAxis, QValue3DAxis
 */

/*!
 * \qmltype AbstractAxis3D
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QAbstract3DAxis
 * \brief AbstractAxis3D is base type for axes of a graph.
 *
 * This type is uncreatable, but contains properties that are exposed via subtypes.
 *
 * For AbstractAxis3D enums, see \l QAbstract3DAxis::AxisOrientation and
 * \l{QAbstract3DAxis::AxisType}.
 */

/*!
 * \qmlproperty string AbstractAxis3D::title
 * Defines the title for the axis.
 *
 * \sa titleVisible, titleFixed
 */

/*!
 * \qmlproperty list AbstractAxis3D::labels
 * Defines the labels for the axis.
 * \note Setting this property for ValueAxis3D does nothing, as it generates labels automatically.
 */

/*!
 * \qmlproperty AbstractAxis3D.AxisOrientation AbstractAxis3D::orientation
 * Defines the orientation of the axis.
 */

/*!
 * \qmlproperty AbstractAxis3D.AxisType AbstractAxis3D::type
 * Defines the type of the axis.
 */

/*!
 * \qmlproperty real AbstractAxis3D::min
 *
 * Defines the minimum value on the axis.
 * When setting this property the max is adjusted if necessary, to ensure that the range remains
 * valid.
 */

/*!
 * \qmlproperty real AbstractAxis3D::max
 *
 * Defines the maximum value on the axis.
 * When setting this property the min is adjusted if necessary, to ensure that the range remains
 * valid.
 */

/*!
 * \qmlproperty bool AbstractAxis3D::autoAdjustRange
 *
 * If set, the axis will automatically adjust the range so that all data fits in it.
 */

/*!
 * \qmlproperty real AbstractAxis3D::labelAutoRotation
 *
 * Defines the maximum \a angle the labels can autorotate when the camera angle changes.
 * The \a angle can be between 0 and 90, inclusive. The default value is 0.
 * If the value is 0, axis labels do not automatically rotate.
 * If the value is greater than zero, labels attempt to orient themselves toward the camera, up to
 * the specified angle.
 */

/*!
 * \qmlproperty bool AbstractAxis3D::titleVisible
 *
 * Defines if the axis title is visible in the primary graph view. The default value is \c{false}.
 *
 * \sa title, titleFixed
 */

/*!
 * \qmlproperty bool AbstractAxis3D::titleFixed
 *
 * If \c{true}, axis titles in the primary graph view will be rotated towards the camera similarly
 * to the axis labels.
 * If \c{false}, axis titles are only rotated around their axis but are not otherwise oriented
 * towards the camera.
 * This property doesn't have any effect if labelAutoRotation property value is zero.
 * Default value is \c{true}.
 *
 * \sa labelAutoRotation, title, titleVisible
 */

/*!
 * \enum QAbstract3DAxis::AxisOrientation
 *
 * The orientation of the axis object.
 *
 * \value AxisOrientationNone
 * \value AxisOrientationX
 * \value AxisOrientationY
 * \value AxisOrientationZ
 */

/*!
 * \enum QAbstract3DAxis::AxisType
 *
 * The type of the axis object.
 *
 * \value AxisTypeNone
 * \value AxisTypeCategory
 * \value AxisTypeValue
 */

/*!
 * \internal
 */
QAbstract3DAxis::QAbstract3DAxis(QAbstract3DAxisPrivate *d, QObject *parent) :
    QObject(parent),
    d_ptr(d)
{
}

/*!
 * Destroys QAbstract3DAxis.
 */
QAbstract3DAxis::~QAbstract3DAxis()
{
}

/*!
 * \property QAbstract3DAxis::orientation
 *
 * Defines the orientation of the axis, one of AxisOrientation.
 */
QAbstract3DAxis::AxisOrientation QAbstract3DAxis::orientation() const
{
    return d_ptr->m_orientation;
}

/*!
 * \property QAbstract3DAxis::type
 *
 * Defines the type of the axis, one of AxisType.
 */
QAbstract3DAxis::AxisType QAbstract3DAxis::type() const
{
    return d_ptr->m_type;
}

/*!
 * \property QAbstract3DAxis::title
 *
 * Defines the title for the axis.
 *
 * \sa titleVisible, titleFixed
 */
void QAbstract3DAxis::setTitle(const QString &title)
{
    if (d_ptr->m_title != title) {
        d_ptr->m_title = title;
        emit titleChanged(title);
    }
}

QString QAbstract3DAxis::title() const
{
    return d_ptr->m_title;
}

/*!
 * \property QAbstract3DAxis::labels
 *
 * Defines the labels for the axis.
 * \note Setting this property for QValue3DAxis does nothing, as it generates labels automatically.
 */
void QAbstract3DAxis::setLabels(const QStringList &labels)
{
    Q_UNUSED(labels)
}

QStringList QAbstract3DAxis::labels() const
{
    d_ptr->updateLabels();
    return d_ptr->m_labels;
}

/*!
 * Sets value range of the axis from \a min to \a max.
 * When setting the range, the max is adjusted if necessary, to ensure that the range remains valid.
 * \note For QCategory3DAxis this specifies the index range of rows or columns to show.
 */
void QAbstract3DAxis::setRange(float min, float max)
{
    d_ptr->setRange(min, max);
    setAutoAdjustRange(false);
}

/*!
 * \property QAbstract3DAxis::labelAutoRotation
 *
 * Defines the maximum \a angle the labels can autorotate when the camera angle changes.
 * The \a angle can be between 0 and 90, inclusive. The default value is 0.
 * If the value is 0, axis labels do not automatically rotate.
 * If the value is greater than zero, labels attempt to orient themselves toward the camera, up to
 * the specified angle.
 */
void QAbstract3DAxis::setLabelAutoRotation(float angle)
{
    if (angle < 0.0f)
        angle = 0.0f;
    if (angle > 90.0f)
        angle = 90.0f;
    if (d_ptr->m_labelAutoRotation != angle) {
        d_ptr->m_labelAutoRotation = angle;
        emit labelAutoRotationChanged(angle);
    }
}

float QAbstract3DAxis::labelAutoRotation() const
{
    return d_ptr->m_labelAutoRotation;
}

/*!
 * \property QAbstract3DAxis::titleVisible
 *
 * Defines if the axis title is visible in the primary graph view. The default value is \c{false}.
 *
 * \sa title, titleFixed
 */
void QAbstract3DAxis::setTitleVisible(bool visible)
{
    if (d_ptr->m_titleVisible != visible) {
        d_ptr->m_titleVisible = visible;
        emit titleVisibilityChanged(visible);
    }
}

bool QAbstract3DAxis::isTitleVisible() const
{
    return d_ptr->m_titleVisible;
}

/*!
 * \property QAbstract3DAxis::titleFixed
 *
 * If \c{true}, axis titles in the primary graph view will be rotated towards the camera similarly
 * to the axis labels.
 * If \c{false}, axis titles are only rotated around their axis but are not otherwise oriented
 * towards the camera.
 * This property doesn't have any effect if labelAutoRotation property value is zero.
 * Default value is \c{true}.
 *
 * \sa labelAutoRotation, title, titleVisible
 */
void QAbstract3DAxis::setTitleFixed(bool fixed)
{
    if (d_ptr->m_titleFixed != fixed) {
        d_ptr->m_titleFixed = fixed;
        emit titleFixedChanged(fixed);
    }
}

bool QAbstract3DAxis::isTitleFixed() const
{
    return d_ptr->m_titleFixed;
}

/*!
 * \property QAbstract3DAxis::min
 *
 * Defines the minimum value on the axis.
 * When setting this property the max is adjusted if necessary, to ensure that the range remains
 * valid.
 * \note For QCategory3DAxis this specifies the index of the first row or column to show.
 */
void QAbstract3DAxis::setMin(float min)
{
    d_ptr->setMin(min);
    setAutoAdjustRange(false);
}

/*!
 * \property QAbstract3DAxis::max
 *
 * Defines the maximum value on the axis.
 * When setting this property the min is adjusted if necessary, to ensure that the range remains
 * valid.
 * \note For QCategory3DAxis this specifies the index of the last row or column to show.
 */
void QAbstract3DAxis::setMax(float max)
{
    d_ptr->setMax(max);
    setAutoAdjustRange(false);
}

float QAbstract3DAxis::min() const
{
    return d_ptr->m_min;
}

float QAbstract3DAxis::max() const
{
    return d_ptr->m_max;
}

/*!
 * \property QAbstract3DAxis::autoAdjustRange
 *
 * If set, the axis will automatically adjust the range so that all data fits in it.
 *
 * \sa setRange(), setMin(), setMax()
 */
void QAbstract3DAxis::setAutoAdjustRange(bool autoAdjust)
{
    if (d_ptr->m_autoAdjust != autoAdjust) {
        d_ptr->m_autoAdjust = autoAdjust;
        emit autoAdjustRangeChanged(autoAdjust);
    }
}

bool QAbstract3DAxis::isAutoAdjustRange() const
{
    return d_ptr->m_autoAdjust;
}

/*!
 * \fn QAbstract3DAxis::rangeChanged(float min, float max)
 *
 * Emits range \a min and \a max values when range changes.
 */

// QAbstract3DAxisPrivate
QAbstract3DAxisPrivate::QAbstract3DAxisPrivate(QAbstract3DAxis *q, QAbstract3DAxis::AxisType type)
    : QObject(0),
      q_ptr(q),
      m_orientation(QAbstract3DAxis::AxisOrientationNone),
      m_type(type),
      m_isDefaultAxis(false),
      m_min(0.0f),
      m_max(10.0f),
      m_autoAdjust(true),
      m_labelAutoRotation(0.0f),
      m_titleVisible(false),
      m_titleFixed(true)
{
}

QAbstract3DAxisPrivate::~QAbstract3DAxisPrivate()
{
}

void QAbstract3DAxisPrivate::setOrientation(QAbstract3DAxis::AxisOrientation orientation)
{
    if (m_orientation == QAbstract3DAxis::AxisOrientationNone) {
        m_orientation = orientation;
        emit q_ptr->orientationChanged(orientation);
    } else {
        Q_ASSERT("Attempted to reset axis orientation.");
    }
}

void QAbstract3DAxisPrivate::updateLabels()
{
    // Default implementation does nothing
}

void QAbstract3DAxisPrivate::setRange(float min, float max, bool suppressWarnings)
{
    bool adjusted = false;
    if (!allowNegatives()) {
        if (allowZero()) {
            if (min < 0.0f) {
                min = 0.0f;
                adjusted = true;
            }
            if (max < 0.0f) {
                max = 0.0f;
                adjusted = true;
            }
        } else {
            if (min <= 0.0f) {
                min = 1.0f;
                adjusted = true;
            }
            if (max <= 0.0f) {
                max = 1.0f;
                adjusted = true;
            }
        }
    }
    // If min >= max, we adjust ranges so that
    // m_max becomes (min + 1.0f)
    // as axes need some kind of valid range.
    bool minDirty = false;
    bool maxDirty = false;
    if (m_min != min) {
        m_min = min;
        minDirty = true;
    }
    if (m_max != max || min > max || (!allowMinMaxSame() && min == max)) {
        if (min > max || (!allowMinMaxSame() && min == max)) {
            m_max = min + 1.0f;
            adjusted = true;
        } else {
            m_max = max;
        }
        maxDirty = true;
    }

    if (minDirty || maxDirty) {
        if (adjusted && !suppressWarnings) {
            qWarning() << "Warning: Tried to set invalid range for axis."
                          " Range automatically adjusted to a valid one:"
                       << min << "-" << max << "-->" << m_min << "-" << m_max;
        }
        emit q_ptr->rangeChanged(m_min, m_max);
    }

    if (minDirty)
        emit q_ptr->minChanged(m_min);
    if (maxDirty)
        emit q_ptr->maxChanged(m_max);
}

void QAbstract3DAxisPrivate::setMin(float min)
{
    if (!allowNegatives()) {
        if (allowZero()) {
            if (min < 0.0f) {
                min = 0.0f;
                qWarning() << "Warning: Tried to set negative minimum for an axis that only"
                              "supports positive values and zero:" << min;
            }
        } else {
            if (min <= 0.0f) {
                min = 1.0f;
                qWarning() << "Warning: Tried to set negative or zero minimum for an axis that only"
                              "supports positive values:" << min;
            }
        }
    }

    if (m_min != min) {
        bool maxChanged = false;
        if (min > m_max || (!allowMinMaxSame() && min == m_max)) {
            float oldMax = m_max;
            m_max = min + 1.0f;
            qWarning() << "Warning: Tried to set minimum to equal or larger than maximum for"
                          " value axis. Maximum automatically adjusted to a valid one:"
                       << oldMax <<  "-->" << m_max;
            maxChanged = true;
        }
        m_min = min;

        emit q_ptr->rangeChanged(m_min, m_max);
        emit q_ptr->minChanged(m_min);
        if (maxChanged)
            emit q_ptr->maxChanged(m_max);
    }
}

void QAbstract3DAxisPrivate::setMax(float max)
{
    if (!allowNegatives()) {
        if (allowZero()) {
            if (max < 0.0f) {
                max = 0.0f;
                qWarning() << "Warning: Tried to set negative maximum for an axis that only"
                              "supports positive values and zero:" << max;
            }
        } else {
            if (max <= 0.0f) {
                max = 1.0f;
                qWarning() << "Warning: Tried to set negative or zero maximum for an axis that only"
                              "supports positive values:" << max;
            }
        }
    }

    if (m_max != max) {
        bool minChanged = false;
        if (m_min > max || (!allowMinMaxSame() && m_min == max)) {
            float oldMin = m_min;
            m_min = max - 1.0f;
            if (!allowNegatives() && m_min < 0.0f) {
                if (allowZero())
                    m_min = 0.0f;
                else
                    m_min = max / 2.0f; // Need some positive value smaller than max

                if (!allowMinMaxSame() && max == 0.0f) {
                    m_min = oldMin;
                    qWarning() << "Unable to set maximum value to zero.";
                    return;
                }
            }
            qWarning() << "Warning: Tried to set maximum to equal or smaller than minimum for"
                          " value axis. Minimum automatically adjusted to a valid one:"
                       << oldMin <<  "-->" << m_min;
            minChanged = true;
        }
        m_max = max;
        emit q_ptr->rangeChanged(m_min, m_max);
        emit q_ptr->maxChanged(m_max);
        if (minChanged)
            emit q_ptr->minChanged(m_min);
    }
}

QT_END_NAMESPACE_DATAVISUALIZATION
