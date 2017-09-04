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

#include <private/xyanimation_p.h>
#include <private/xychart_p.h>
#include <QtCore/QDebug>

Q_DECLARE_METATYPE(QVector<QPointF>)

QT_CHARTS_BEGIN_NAMESPACE

XYAnimation::XYAnimation(XYChart *item, int duration, QEasingCurve &curve)
    : ChartAnimation(item),
      m_type(NewAnimation),
      m_dirty(false),
      m_index(-1),
      m_item(item)
{
    setDuration(duration);
    setEasingCurve(curve);
}

XYAnimation::~XYAnimation()
{
}

void XYAnimation::setup(const QVector<QPointF> &oldPoints, const QVector<QPointF> &newPoints, int index)
{
    m_type = NewAnimation;

    if (state() != QAbstractAnimation::Stopped) {
        stop();
        m_dirty = false;
    }

    if (!m_dirty) {
        m_dirty = true;
        m_oldPoints = oldPoints;
    }

    m_newPoints = newPoints;

    int x = m_oldPoints.count();
    int y = m_newPoints.count();
    int diff = x - y;
    int requestedDiff = oldPoints.count() - y;

    // m_oldPoints can be whatever between 0 and actual points count if new animation setup
    // interrupts a previous animation, so only do remove and add animations if both
    // stored diff and requested diff indicate add or remove. Also ensure that index is not
    // invalid.
    if (diff == 1 && requestedDiff == 1 && index >= 0 && y > 0 && index <= y) {
        //remove point
        m_newPoints.insert(index, index > 0 ? newPoints[index - 1] : newPoints[index]);
        m_index = index;
        m_type = RemovePointAnimation;
    }

    if (diff == -1 && requestedDiff == -1 && index >= 0 && index <= x) {
        //add point
        m_oldPoints.insert(index, index > 0 ? newPoints[index - 1] : newPoints[index]);
        m_index = index;
        m_type = AddPointAnimation;
    }

    x = m_oldPoints.count();
    y = m_newPoints.count();

    if (x != y)
        m_type = NewAnimation;
    else if (m_type == NewAnimation)
        m_type = ReplacePointAnimation;

    setKeyValueAt(0.0, qVariantFromValue(m_oldPoints));
    setKeyValueAt(1.0, qVariantFromValue(m_newPoints));
}

QVariant XYAnimation::interpolated(const QVariant &start, const QVariant &end, qreal progress) const
{
    QVector<QPointF> startVector = qvariant_cast<QVector<QPointF> >(start);
    QVector<QPointF> endVector = qvariant_cast<QVector<QPointF> >(end);
    QVector<QPointF> result;

    switch (m_type) {

    case ReplacePointAnimation:
    case AddPointAnimation:
    case RemovePointAnimation: {
        if (startVector.count() != endVector.count())
            break;

        for (int i = 0; i < startVector.count(); i++) {
            qreal x = startVector[i].x() + ((endVector[i].x() - startVector[i].x()) * progress);
            qreal y = startVector[i].y() + ((endVector[i].y() - startVector[i].y()) * progress);
            result << QPointF(x, y);
        }

    }
    break;
    case NewAnimation: {
        for (int i = 0; i < endVector.count() * qBound(qreal(0), progress, qreal(1)); i++)
            result << endVector[i];
    }
    break;
    default:
        qWarning() << "Unknown type of animation";
        break;
    }

    return qVariantFromValue(result);
}

void XYAnimation::updateCurrentValue(const QVariant &value)
{
    if (state() != QAbstractAnimation::Stopped) { //workaround

        QVector<QPointF> vector = qvariant_cast<QVector<QPointF> >(value);
        m_item->setGeometryPoints(vector);
        m_item->updateGeometry();
        m_item->setDirty(true);
        m_dirty = false;

    }
}

void XYAnimation::updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
{
    if (oldState == QAbstractAnimation::Running && newState == QAbstractAnimation::Stopped) {
        if (m_item->isDirty() && m_type == RemovePointAnimation) {
            if (!m_newPoints.isEmpty())
                m_newPoints.remove(m_index);
            m_item->setGeometryPoints(m_newPoints);
        }
    }
}

#include "moc_chartanimation_p.cpp"
QT_CHARTS_END_NAMESPACE
