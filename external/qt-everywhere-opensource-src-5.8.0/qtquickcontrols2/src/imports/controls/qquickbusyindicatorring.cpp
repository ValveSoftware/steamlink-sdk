/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qquickbusyindicatorring_p.h"

#include <QtCore/qset.h>
#include <QtGui/qpainter.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/qsgnode.h>
#include <QtQuick/qquickwindow.h>
#include <QtQuick/qsgrectanglenode.h>

QT_BEGIN_NAMESPACE

class QQuickBusyIndicatorAnimatorJob : public QQuickAnimatorJob
{
public:
    QQuickBusyIndicatorAnimatorJob();
    ~QQuickBusyIndicatorAnimatorJob();

    void initialize(QQuickAnimatorController *controller) override;
    void updateCurrentTime(int time) override;
    void writeBack() override;
    void nodeWasDestroyed() override;
    void afterNodeSync() override;

private:
    QSGNode *m_node;
};

static const int circles = 10;
static const int animationDuration = 100 * circles * 2;

QQuickBusyIndicatorRing::QQuickBusyIndicatorRing(QQuickItem *parent) :
    QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents);
    setImplicitWidth(116);
    setImplicitHeight(116);
}

QQuickBusyIndicatorRing::~QQuickBusyIndicatorRing()
{
}

static QPointF moveBy(const QPointF &pos, qreal rotation, qreal distance)
{
    return pos - QTransform().rotate(rotation).map(QPointF(0, distance));
}

QSGNode *QQuickBusyIndicatorRing::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QQuickItemPrivate *d = QQuickItemPrivate::get(this);

    if (!oldNode) {
        oldNode = window()->createRectangleNode();
        static_cast<QSGRectangleNode *>(oldNode)->setColor(Qt::transparent);
    }
    static_cast<QSGRectangleNode *>(oldNode)->setRect(boundingRect());

    QSGTransformNode *rootTransformNode = static_cast<QSGTransformNode *>(oldNode->firstChild());
    if (!rootTransformNode) {
        rootTransformNode = new QSGTransformNode;
        oldNode->appendChildNode(rootTransformNode);
    }
    Q_ASSERT(rootTransformNode->type() == QSGNode::TransformNodeType);

    const qreal w = width();
    const qreal h = height();
    const qreal sz = qMin(w, h);
    const qreal dx = (w - sz) / 2;
    const qreal dy = (h - sz) / 2;
    const int circleRadius = sz / 12;
    const QColor color(0x35, 0x36, 0x37);

    QSGTransformNode *transformNode = static_cast<QSGTransformNode *>(rootTransformNode->firstChild());
    for (int i = 0; i < circles; ++i) {
        if (!transformNode) {
            transformNode = new QSGTransformNode;
            rootTransformNode->appendChildNode(transformNode);

            QSGOpacityNode *opacityNode = new QSGOpacityNode;
            transformNode->appendChildNode(opacityNode);

            QSGInternalRectangleNode *rectNode = d->sceneGraphContext()->createInternalRectangleNode();
            rectNode->setAntialiasing(true);
            rectNode->setColor(color);
            rectNode->setPenColor(color);
            opacityNode->appendChildNode(rectNode);
        }

        QSGNode *opacityNode = transformNode->firstChild();
        Q_ASSERT(opacityNode->type() == QSGNode::OpacityNodeType);

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(opacityNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        QPointF pos = QPointF(sz / 2 - circleRadius, sz / 2 - circleRadius);
        pos = moveBy(pos, 360 / circles * i, sz / 2 - circleRadius);

        QMatrix4x4 m;
        m.translate(dx + pos.x(), dy + pos.y());
        transformNode->setMatrix(m);

        rectNode->setRect(QRectF(QPointF(), QSizeF(circleRadius * 2, circleRadius * 2)));
        rectNode->setRadius(circleRadius);
        rectNode->update();

        transformNode = static_cast<QSGTransformNode *>(transformNode->nextSibling());
    }

    return oldNode;
}

QQuickBusyIndicatorAnimator::QQuickBusyIndicatorAnimator(QObject *parent) :
    QQuickAnimator(parent)
{
    setDuration(animationDuration);
    setLoops(QQuickAnimator::Infinite);
}

QString QQuickBusyIndicatorAnimator::propertyName() const
{
    return QString();
}

QQuickAnimatorJob *QQuickBusyIndicatorAnimator::createJob() const
{
    return new QQuickBusyIndicatorAnimatorJob;
}

QQuickBusyIndicatorAnimatorJob::QQuickBusyIndicatorAnimatorJob() : m_node(nullptr)
{
}

QQuickBusyIndicatorAnimatorJob::~QQuickBusyIndicatorAnimatorJob()
{
}

void QQuickBusyIndicatorAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    QQuickAnimatorJob::initialize(controller);
    m_node = QQuickItemPrivate::get(m_target)->childContainerNode();
}

void QQuickBusyIndicatorAnimatorJob::updateCurrentTime(int time)
{
    if (!m_node)
        return;

    QSGRectangleNode *rootRectNode = static_cast<QSGRectangleNode *>(m_node->firstChild());
    if (!rootRectNode)
        return;

    Q_ASSERT(rootRectNode->type() == QSGNode::GeometryNodeType);

    QSGTransformNode *rootTransformNode = static_cast<QSGTransformNode*>(rootRectNode->firstChild());
    Q_ASSERT(rootTransformNode->type() == QSGNode::TransformNodeType);

    const qreal percentageComplete = time / qreal(animationDuration);
    const qreal firstPhaseProgress = percentageComplete <= 0.5 ? percentageComplete * 2 : 0;
    const qreal secondPhaseProgress = percentageComplete > 0.5 ? (percentageComplete - 0.5) * 2 : 0;

    QSGTransformNode *transformNode = static_cast<QSGTransformNode*>(rootTransformNode->firstChild());
    const QColor color(0x35, 0x36, 0x37);
    const QColor transparent(Qt::transparent);
    Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);
    for (int i = 0; i < circles; ++i) {
        QSGOpacityNode *opacityNode = static_cast<QSGOpacityNode*>(transformNode->firstChild());
        Q_ASSERT(opacityNode->type() == QSGNode::OpacityNodeType);

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode*>(opacityNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        const bool fill = (firstPhaseProgress > qreal(i) / circles) || (secondPhaseProgress > 0 && secondPhaseProgress < qreal(i) / circles);
        rectNode->setPenWidth(fill ? 0 : 1);
        rectNode->setColor(fill ? color : transparent);
        rectNode->update();

        transformNode = static_cast<QSGTransformNode*>(transformNode->nextSibling());
    }
}

void QQuickBusyIndicatorAnimatorJob::writeBack()
{
}

void QQuickBusyIndicatorAnimatorJob::nodeWasDestroyed()
{
    m_node = nullptr;
}

void QQuickBusyIndicatorAnimatorJob::afterNodeSync()
{
    m_node = QQuickItemPrivate::get(m_target)->childContainerNode();
}

QT_END_NAMESPACE
