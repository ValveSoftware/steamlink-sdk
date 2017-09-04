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

#include <private/splineanimation_p.h>
#include <private/splinechartitem_p.h>
#include <QtCore/QDebug>

Q_DECLARE_METATYPE(QVector<QPointF>)
Q_DECLARE_METATYPE(SplineVector)

QT_CHARTS_BEGIN_NAMESPACE

SplineAnimation::SplineAnimation(SplineChartItem *item, int duration, QEasingCurve &curve)
    : XYAnimation(item, duration, curve),
      m_item(item),
      m_valid(false)
{
}

SplineAnimation::~SplineAnimation()
{
}

void SplineAnimation::setup(QVector<QPointF> &oldPoints, QVector<QPointF> &newPoints, QVector<QPointF> &oldControlPoints, QVector<QPointF> &newControlPoints, int index)
{
    if (newPoints.count() * 2 - 2 != newControlPoints.count() || newControlPoints.count() < 2) {
        m_valid = false;
        m_dirty = false;
        m_item->setGeometryPoints(newPoints);
        m_item->setControlGeometryPoints(newControlPoints);
        m_item->setDirty(false);
        m_item->updateGeometry();
        return;
    }

    m_type = NewAnimation;

    if (state() != QAbstractAnimation::Stopped) {
        stop();
        m_dirty = false;
    }

    if (!m_dirty) {
        m_dirty = true;
        m_oldSpline.first = oldPoints;
        m_oldSpline.second = oldControlPoints;
    }

    m_newSpline.first = newPoints;
    m_newSpline.second = newControlPoints;


    int x = m_oldSpline.first.count();
    int y = m_newSpline.first.count();

    if (x - y == 1 && index >= 0 && y > 0) {
        //remove point
        if (index > 0) {
            m_newSpline.first.insert(index, newPoints[index - 1]);
            m_newSpline.second.insert((index - 1) * 2, newPoints[index - 1]);
            m_newSpline.second.insert((index - 1) * 2 + 1, newPoints[index - 1]);
        } else {
            m_newSpline.first.insert(0, newPoints[index]);
            m_newSpline.second.insert(0, newPoints[index]);
            m_newSpline.second.insert(1, newPoints[index]);
        }
        m_index = index;
        m_type = RemovePointAnimation;
    }

    if (x - y == -1 && index >= 0) {
        //add point
        if (index > 0) {
            m_oldSpline.first.insert(index, newPoints[index - 1]);
            m_oldSpline.second.insert((index - 1) * 2, newPoints[index - 1]);
            m_oldSpline.second.insert((index - 1) * 2 + 1, newPoints[index - 1]);
        } else {
            m_oldSpline.first.insert(0, newPoints[index]);
            m_oldSpline.second.insert(0, newPoints[index]);
            m_oldSpline.second.insert(1, newPoints[index]);
        }
        m_index = index;
        m_type = AddPointAnimation;
    }

    x = m_oldSpline.first.count();
    y = m_newSpline.first.count();

    if (x != y) {
        m_type = NewAnimation;
    } else if (m_type == NewAnimation) {
        m_type = ReplacePointAnimation;
    }


    setKeyValueAt(0.0, qVariantFromValue(m_oldSpline));
    setKeyValueAt(1.0, qVariantFromValue(m_newSpline));

    m_valid = true;

}

QVariant SplineAnimation::interpolated(const QVariant &start, const QVariant &end, qreal progress) const
{

    SplineVector startPair = qvariant_cast< SplineVector >(start);
    SplineVector endPair = qvariant_cast< SplineVector >(end);
    SplineVector result;

    switch (animationType()) {
    case RemovePointAnimation:
    case AddPointAnimation:
    case ReplacePointAnimation: {
        if (startPair.first.count() != endPair.first.count())
            break;
        Q_ASSERT(startPair.first.count() * 2 - 2 == startPair.second.count());
        Q_ASSERT(endPair.first.count() * 2 - 2 == endPair.second.count());
        for (int i = 0; i < endPair.first.count(); i++) {
            qreal x = startPair.first[i].x() + ((endPair.first[i].x() - startPair.first[i].x()) * progress);
            qreal y = startPair.first[i].y() + ((endPair.first[i].y() - startPair.first[i].y()) * progress);
            result.first << QPointF(x, y);
            if (i + 1 >= endPair.first.count())
                continue;
            x = startPair.second[i * 2].x() + ((endPair.second[i * 2].x() - startPair.second[i * 2].x()) * progress);
            y = startPair.second[i * 2].y() + ((endPair.second[i * 2].y() - startPair.second[i * 2].y()) * progress);
            result.second << QPointF(x, y);
            x = startPair.second[i * 2 + 1].x() + ((endPair.second[i * 2 + 1].x() - startPair.second[i * 2 + 1].x()) * progress);
            y = startPair.second[i * 2 + 1].y() + ((endPair.second[i * 2 + 1].y() - startPair.second[i * 2 + 1].y()) * progress);
            result.second << QPointF(x, y);
        }
    }
    break;
    case NewAnimation: {
        Q_ASSERT(endPair.first.count() * 2 - 2 == endPair.second.count());
        int count = endPair.first.count() * qBound(qreal(0), progress, qreal(1));
        for (int i = 0; i < count; i++) {
            result.first << endPair.first[i];
            if (i + 1 == count)
                break;
            result.second << endPair.second[2 * i];
            result.second << endPair.second[2 * i + 1];
        }
    }
    break;
    default:
        qWarning() << "Unknown type of animation";
        break;
    }

    return qVariantFromValue(result);
}

void SplineAnimation::updateCurrentValue(const QVariant &value)
{
    if (state() != QAbstractAnimation::Stopped && m_valid) { //workaround
        QPair<QVector<QPointF >, QVector<QPointF > > pair = qvariant_cast< QPair< QVector<QPointF>, QVector<QPointF> > >(value);
        m_item->setGeometryPoints(pair.first);
        m_item->setControlGeometryPoints(pair.second);
        m_item->updateGeometry();
        m_item->setDirty(true);
        m_dirty = false;
    }
}

void SplineAnimation::updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
{
    XYAnimation::updateState(newState, oldState);

    if (oldState == QAbstractAnimation::Running && newState == QAbstractAnimation::Stopped) {
        if (m_item->isDirty() && m_type == RemovePointAnimation) {
            if (!m_newSpline.first.isEmpty()) {
                if (m_index) {
                    m_newSpline.first.remove(m_index);
                    m_newSpline.second.remove((m_index - 1) * 2);
                    m_newSpline.second.remove((m_index - 1) * 2);
                } else {
                    m_newSpline.first.remove(0);
                    m_newSpline.second.remove(0);
                    m_newSpline.second.remove(0);
                }
            }
            m_item->setGeometryPoints(m_newSpline.first);
            m_item->setControlGeometryPoints(m_newSpline.second);
        }
    }

    if (oldState == QAbstractAnimation::Stopped && newState == QAbstractAnimation::Running) {
        if (!m_valid)
            stop();
    }
}

QT_CHARTS_END_NAMESPACE
