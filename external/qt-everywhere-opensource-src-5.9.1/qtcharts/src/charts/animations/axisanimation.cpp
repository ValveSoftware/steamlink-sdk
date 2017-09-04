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

#include <private/axisanimation_p.h>
#include <private/chartaxiselement_p.h>
#include <private/qabstractaxis_p.h>

Q_DECLARE_METATYPE(QVector<qreal>)

QT_CHARTS_BEGIN_NAMESPACE


AxisAnimation::AxisAnimation(ChartAxisElement *axis, int duration, QEasingCurve &curve)
    : ChartAnimation(axis),
      m_axis(axis),
      m_type(DefaultAnimation)
{
    setDuration(duration);
    setEasingCurve(curve);
}

AxisAnimation::~AxisAnimation()
{
}

void AxisAnimation::setAnimationType(Animation type)
{
    if (state() != QAbstractAnimation::Stopped)
        stop();
    m_type = type;
}

void AxisAnimation::setAnimationPoint(const QPointF &point)
{
    if (state() != QAbstractAnimation::Stopped)
        stop();
    m_point = point;
}

void AxisAnimation::setValues(QVector<qreal> &oldLayout, QVector<qreal> &newLayout)
{
    if (state() != QAbstractAnimation::Stopped) stop();

    switch (m_type) {
    case ZoomOutAnimation: {
        QRectF rect = m_axis->gridGeometry();
        oldLayout.resize(newLayout.count());

        for (int i = 0, j = oldLayout.count() - 1; i < (oldLayout.count() + 1) / 2; ++i, --j) {
            oldLayout[i] = m_axis->axis()->orientation() == Qt::Horizontal ? rect.left() : rect.bottom();
            oldLayout[j] = m_axis->axis()->orientation() == Qt::Horizontal ? rect.right() : rect.top();
        }
    }
    break;
    case ZoomInAnimation: {
        int index = qMin(oldLayout.count() * (m_axis->axis()->orientation() == Qt::Horizontal ? m_point.x() : (1 - m_point.y())), newLayout.count() - (qreal)1.0);
        oldLayout.resize(newLayout.count());

        if (index < 0)
            break;
        for (int i = 0; i < oldLayout.count(); i++)
            oldLayout[i] = oldLayout[index];
    }
    break;
    case MoveForwardAnimation: {
        oldLayout.resize(newLayout.count());

        for (int i = 0, j = i + 1; i < oldLayout.count() - 1; ++i, ++j)
            oldLayout[i] = oldLayout[j];
    }
    break;
    case MoveBackwordAnimation: {
        oldLayout.resize(newLayout.count());

        for (int i = oldLayout.count() - 1, j = i - 1; i > 0; --i, --j)
            oldLayout[i] = oldLayout[j];
    }
    break;
    default: {
        oldLayout.resize(newLayout.count());
        QRectF rect = m_axis->gridGeometry();
        for (int i = 0, j = oldLayout.count() - 1; i < oldLayout.count(); ++i, --j)
            oldLayout[i] = m_axis->axis()->orientation() == Qt::Horizontal ? rect.left() : rect.top();
    }
    break;
    }

    QVariantAnimation::KeyValues value;
    setKeyValues(value); //workaround for wrong interpolation call
    setKeyValueAt(0.0, qVariantFromValue(oldLayout));
    setKeyValueAt(1.0, qVariantFromValue(newLayout));
}

QVariant AxisAnimation::interpolated(const QVariant &start, const QVariant &end, qreal progress) const
{
    QVector<qreal> startVector = qvariant_cast<QVector<qreal> >(start);
    QVector<qreal> endVecotr = qvariant_cast<QVector<qreal> >(end);
    QVector<qreal> result;

    Q_ASSERT(startVector.count() == endVecotr.count()) ;

    for (int i = 0; i < startVector.count(); i++) {
        qreal value = startVector[i] + ((endVecotr[i] - startVector[i]) * progress);
        result << value;
    }
    return qVariantFromValue(result);
}


void AxisAnimation::updateCurrentValue(const QVariant &value)
{
    if (state() != QAbstractAnimation::Stopped) { //workaround
        QVector<qreal> vector = qvariant_cast<QVector<qreal> >(value);
        m_axis->setLayout(vector);
        m_axis->updateGeometry();
    }

}

QT_CHARTS_END_NAMESPACE
