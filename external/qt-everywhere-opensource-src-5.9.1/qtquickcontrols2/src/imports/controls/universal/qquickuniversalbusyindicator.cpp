/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickuniversalbusyindicator_p.h"

#include <QtCore/qmath.h>
#include <QtCore/qeasingcurve.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuickControls2/private/qquickanimatednode_p.h>

QT_BEGIN_NAMESPACE

static const int PhaseCount = 6;
static const int Interval = 167;
static const int TotalDuration = 4052;

class QQuickUniversalBusyIndicatorNode : public QQuickAnimatedNode
{
public:
    QQuickUniversalBusyIndicatorNode(QQuickUniversalBusyIndicator *item);

    void updateCurrentTime(int time) override;
    void sync(QQuickItem *item) override;

private:
    struct Phase {
        Phase() : duration(0), from(0), to(0), curve(QEasingCurve::Linear) { }
        Phase(int d, qreal f, qreal t, QEasingCurve::Type c) : duration(d), from(f), to(t), curve(c) { }
        int duration;
        qreal from;
        qreal to;
        QEasingCurve curve;
    };

    Phase m_phases[PhaseCount];
};

QQuickUniversalBusyIndicatorNode::QQuickUniversalBusyIndicatorNode(QQuickUniversalBusyIndicator *item)
    : QQuickAnimatedNode(item)
{
    setLoopCount(Infinite);
    setDuration(TotalDuration);
    setCurrentTime(item->elapsed());

    m_phases[0] = Phase(433, -110,  10, QEasingCurve::BezierSpline);
    m_phases[1] = Phase(767,   10,  93, QEasingCurve::Linear      );
    m_phases[2] = Phase(417,   93, 205, QEasingCurve::BezierSpline);
    m_phases[3] = Phase(400,  205, 357, QEasingCurve::BezierSpline);
    m_phases[4] = Phase(766,  357, 439, QEasingCurve::Linear      );
    m_phases[5] = Phase(434,  439, 585, QEasingCurve::BezierSpline);

    m_phases[0].curve.addCubicBezierSegment(QPointF(0.02, 0.33), QPointF(0.38, 0.77), QPointF(1.00, 1.00));
    m_phases[2].curve.addCubicBezierSegment(QPointF(0.57, 0.17), QPointF(0.95, 0.75), QPointF(1.00, 1.00));
    m_phases[3].curve.addCubicBezierSegment(QPointF(0.00, 0.19), QPointF(0.07, 0.72), QPointF(1.00, 1.00));
    m_phases[5].curve.addCubicBezierSegment(QPointF(0.00, 0.00), QPointF(0.95, 0.37), QPointF(1.00, 1.00));
}

void QQuickUniversalBusyIndicatorNode::updateCurrentTime(int time)
{
    int nodeIndex = 0;
    int count = childCount();
    QSGTransformNode *transformNode = static_cast<QSGTransformNode *>(firstChild());
    while (transformNode) {
        Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);

        QSGOpacityNode *opacityNode = static_cast<QSGOpacityNode *>(transformNode->firstChild());
        Q_ASSERT(opacityNode->type() == QSGNode::OpacityNodeType);

        int begin = nodeIndex * Interval;
        int end = TotalDuration - (PhaseCount - nodeIndex - 1) * Interval;

        bool visible = time >= begin && time <= end;
        opacityNode->setOpacity(visible ? 1.0 : 0.0);

        if (visible) {
            int phaseIndex, remain = time, elapsed = 0;
            for (phaseIndex = 0; phaseIndex < PhaseCount - 1; ++phaseIndex) {
                if (remain <= m_phases[phaseIndex].duration + begin)
                    break;
                remain -= m_phases[phaseIndex].duration;
                elapsed += m_phases[phaseIndex].duration;
            }

            const Phase &phase = m_phases[phaseIndex];

            qreal from = phase.from - nodeIndex * count;
            qreal to = phase.to - nodeIndex * count;
            qreal pos = time - elapsed - begin;

            qreal value = phase.curve.valueForProgress(pos / phase.duration);
            qreal rotation = from + (to - from) * value;

            QMatrix4x4 matrix;
            matrix.rotate(rotation, 0, 0, 1);
            transformNode->setMatrix(matrix);
        }

        transformNode = static_cast<QSGTransformNode *>(transformNode->nextSibling());
        ++nodeIndex;
    }
}

void QQuickUniversalBusyIndicatorNode::sync(QQuickItem *item)
{
    QQuickUniversalBusyIndicator *indicator = static_cast<QQuickUniversalBusyIndicator *>(item);
    QQuickItemPrivate *d = QQuickItemPrivate::get(item);

    QMatrix4x4 matrix;
    matrix.translate(item->width() / 2, item->height() / 2);
    setMatrix(matrix);

    qreal size = qMin(item->width(), item->height());
    qreal diameter = size / 10.0;
    qreal radius = diameter / 2;
    qreal offset = (size - diameter * 2) / M_PI;
    const QRectF rect(offset, offset, diameter, diameter);

    int count = indicator->count();
    QSGNode *transformNode = firstChild();
    for (int i = 0; i < count; ++i) {
        if (!transformNode) {
            transformNode = new QSGTransformNode;
            appendChildNode(transformNode);

            QSGOpacityNode *opacityNode = new QSGOpacityNode;
            transformNode->appendChildNode(opacityNode);

            QSGInternalRectangleNode *rectNode = d->sceneGraphContext()->createInternalRectangleNode();
            rectNode->setAntialiasing(true);
            opacityNode->appendChildNode(rectNode);
        }

        QSGNode *opacityNode = transformNode->firstChild();
        Q_ASSERT(opacityNode->type() == QSGNode::OpacityNodeType);

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(opacityNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        rectNode->setRect(rect);
        rectNode->setColor(indicator->color());
        rectNode->setRadius(radius);
        rectNode->update();

        transformNode = transformNode->nextSibling();
    }

    while (transformNode) {
        QSGNode *nextSibling = transformNode->nextSibling();
        delete transformNode;
        transformNode = nextSibling;
    }
}

QQuickUniversalBusyIndicator::QQuickUniversalBusyIndicator(QQuickItem *parent)
    : QQuickItem(parent), m_count(5), m_elapsed(0), m_color(Qt::black)
{
    setFlag(ItemHasContents);
}

int QQuickUniversalBusyIndicator::count() const
{
    return m_count;
}

void QQuickUniversalBusyIndicator::setCount(int count)
{
    if (m_count == count)
        return;

    m_count = count;
    update();
}

QColor QQuickUniversalBusyIndicator::color() const
{
    return m_color;
}

void QQuickUniversalBusyIndicator::setColor(const QColor &color)
{
    if (m_color == color)
        return;

    m_color = color;
    update();
}

int QQuickUniversalBusyIndicator::elapsed() const
{
    return m_elapsed;
}

void QQuickUniversalBusyIndicator::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged)
        update();
}

QSGNode *QQuickUniversalBusyIndicator::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QQuickUniversalBusyIndicatorNode *node = static_cast<QQuickUniversalBusyIndicatorNode *>(oldNode);
    if (isVisible() && width() > 0 && height() > 0) {
        if (!node) {
            node = new QQuickUniversalBusyIndicatorNode(this);
            node->start();
        }
        node->sync(this);
    } else {
        m_elapsed = node ? node->currentTime() : 0;
        delete node;
        node = nullptr;
    }
    return node;
}

QT_END_NAMESPACE
