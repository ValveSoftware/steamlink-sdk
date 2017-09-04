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

#include "qquickdefaultbusyindicator_p.h"

#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuickControls2/private/qquickanimatednode_p.h>

QT_BEGIN_NAMESPACE

static const int CircleCount = 10;
static const int TotalDuration = 100 * CircleCount * 2;
static const QRgb TransparentColor = 0x00000000;
static const QRgb FillColor = 0xFF353637;

static QPointF moveCircle(const QPointF &pos, qreal rotation, qreal distance)
{
    return pos - QTransform().rotate(rotation).map(QPointF(0, distance));
}

class QQuickDefaultBusyIndicatorNode : public QQuickAnimatedNode
{
public:
    QQuickDefaultBusyIndicatorNode(QQuickDefaultBusyIndicator *item);

    void updateCurrentTime(int time) override;
    void sync(QQuickItem *item) override;
};

QQuickDefaultBusyIndicatorNode::QQuickDefaultBusyIndicatorNode(QQuickDefaultBusyIndicator *item)
    : QQuickAnimatedNode(item)
{
    setLoopCount(Infinite);
    setDuration(TotalDuration);
    setCurrentTime(item->elapsed());

    for (int i = 0; i < CircleCount; ++i) {
        QSGTransformNode *transformNode = new QSGTransformNode;
        appendChildNode(transformNode);

        QQuickItemPrivate *d = QQuickItemPrivate::get(item);
        QSGInternalRectangleNode *rectNode = d->sceneGraphContext()->createInternalRectangleNode();
        rectNode->setAntialiasing(true);
        transformNode->appendChildNode(rectNode);
    }
}

void QQuickDefaultBusyIndicatorNode::updateCurrentTime(int time)
{
    const qreal percentageComplete = time / qreal(TotalDuration);
    const qreal firstPhaseProgress = percentageComplete <= 0.5 ? percentageComplete * 2 : 0;
    const qreal secondPhaseProgress = percentageComplete > 0.5 ? (percentageComplete - 0.5) * 2 : 0;

    QSGTransformNode *transformNode = static_cast<QSGTransformNode*>(firstChild());
    Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);
    for (int i = 0; i < CircleCount; ++i) {
        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode*>(transformNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        const bool fill = (firstPhaseProgress > qreal(i) / CircleCount) || (secondPhaseProgress > 0 && secondPhaseProgress < qreal(i) / CircleCount);
        rectNode->setColor(QColor::fromRgba(fill ? FillColor : TransparentColor));
        rectNode->setPenWidth(fill ? 0 : 1);
        rectNode->update();

        transformNode = static_cast<QSGTransformNode*>(transformNode->nextSibling());
    }
}

void QQuickDefaultBusyIndicatorNode::sync(QQuickItem *item)
{
    const qreal w = item->width();
    const qreal h = item->height();
    const qreal sz = qMin(w, h);
    const qreal dx = (w - sz) / 2;
    const qreal dy = (h - sz) / 2;
    const int circleRadius = sz / 12;

    QSGTransformNode *transformNode = static_cast<QSGTransformNode *>(firstChild());
    for (int i = 0; i < CircleCount; ++i) {
        Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(transformNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        QPointF pos = QPointF(sz / 2 - circleRadius, sz / 2 - circleRadius);
        pos = moveCircle(pos, 360 / CircleCount * i, sz / 2 - circleRadius);

        QMatrix4x4 m;
        m.translate(dx + pos.x(), dy + pos.y());
        transformNode->setMatrix(m);

        rectNode->setRect(QRectF(QPointF(), QSizeF(circleRadius * 2, circleRadius * 2)));
        rectNode->setRadius(circleRadius);

        transformNode = static_cast<QSGTransformNode *>(transformNode->nextSibling());
    }
}

QQuickDefaultBusyIndicator::QQuickDefaultBusyIndicator(QQuickItem *parent) :
    QQuickItem(parent), m_elapsed(0)
{
    setFlag(ItemHasContents);
}

int QQuickDefaultBusyIndicator::elapsed() const
{
    return m_elapsed;
}

void QQuickDefaultBusyIndicator::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged)
        update();
}

QSGNode *QQuickDefaultBusyIndicator::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QQuickDefaultBusyIndicatorNode *node = static_cast<QQuickDefaultBusyIndicatorNode *>(oldNode);
    if (isVisible() && width() > 0 && height() > 0) {
        if (!node) {
            node = new QQuickDefaultBusyIndicatorNode(this);
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
