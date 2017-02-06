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

#include "qquickprogressstrip_p.h"

#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/qsgrectanglenode.h>

QT_BEGIN_NAMESPACE

class QQuickProgressAnimatorJob : public QQuickAnimatorJob
{
public:
    QQuickProgressAnimatorJob();
    ~QQuickProgressAnimatorJob();

    void initialize(QQuickAnimatorController *controller) override;
    void afterNodeSync() override;
    void updateCurrentTime(int time) override;
    void writeBack() override;
    void nodeWasDestroyed() override;

private:
    QSGNode *m_node;
};

QQuickProgressStrip::QQuickProgressStrip(QQuickItem *parent) :
    QQuickItem(parent),
    m_progress(0),
    m_indeterminate(false)
{
    setFlag(QQuickItem::ItemHasContents);
}

QQuickProgressStrip::~QQuickProgressStrip()
{
}

qreal QQuickProgressStrip::progress() const
{
    return m_progress;
}

void QQuickProgressStrip::setProgress(qreal progress)
{
    if (progress == m_progress)
        return;

    m_progress = progress;
    update();
    emit progressChanged();
}

bool QQuickProgressStrip::isIndeterminate() const
{
    return m_indeterminate;
}

void QQuickProgressStrip::setIndeterminate(bool indeterminate)
{
    if (indeterminate == m_indeterminate)
        return;

    m_indeterminate = indeterminate;
    setClip(m_indeterminate);
    update();
    emit indeterminateChanged();
}

static const int blocks = 4;
static const int blockWidth = 16;
static const int blockRestingSpacing = 4;
static const int blockMovingSpacing = 48;
static const int blockSpan = blocks * (blockWidth + blockRestingSpacing) - blockRestingSpacing;
static const int animationDuration = 4000;
static const int secondPhaseStart = animationDuration * 0.4;
static const int thirdPhaseStart = animationDuration * 0.6;

static inline qreal blockStartX(int blockIndex)
{
    return ((blockIndex + 1) * -blockWidth) - (blockIndex * blockMovingSpacing);
}

static inline qreal blockRestX(int blockIndex, qreal availableWidth)
{
    const qreal spanRightEdgePos = availableWidth / 2 + blockSpan / 2;
    return spanRightEdgePos - (blockIndex + 1) * blockWidth - (blockIndex * blockRestingSpacing);
}

static inline qreal blockEndX(int blockIndex, qreal availableWidth)
{
    return availableWidth - blockStartX(blocks - 1 - blockIndex) - blockWidth;
}

QSGNode *QQuickProgressStrip::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
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

    const qreal y = (height() - implicitHeight()) / 2;
    const QColor color(0x35, 0x36, 0x37);
    if (m_indeterminate) {
        if (rootTransformNode->childCount() != blocks) {
            // This was previously a regular progress bar; remove the old nodes.
            rootTransformNode->removeAllChildNodes();
        }

        QSGTransformNode *transformNode = static_cast<QSGTransformNode*>(rootTransformNode->firstChild());
        for (int i = 0; i < blocks; ++i) {
            if (!transformNode) {
                transformNode = new QSGTransformNode;
                rootTransformNode->appendChildNode(transformNode);
            }

            QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode*>(transformNode->firstChild());
            if (!rectNode) {
                rectNode = d->sceneGraphContext()->createInternalRectangleNode();
                rectNode->setColor(color);
                transformNode->appendChildNode(rectNode);
            }

            QMatrix4x4 m;
            m.translate(blockStartX(i), 0);
            transformNode->setMatrix(m);

            rectNode->setRect(QRectF(QPointF(0, y), QSizeF(blockWidth, implicitHeight())));
            rectNode->update();

            transformNode = static_cast<QSGTransformNode *>(transformNode->nextSibling());
        }
    } else {
        if (rootTransformNode->childCount() > 1) {
            // This was previously an indeterminate progress bar; remove the old nodes.
            rootTransformNode->removeAllChildNodes();
        }

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(rootTransformNode->firstChild());
        if (!rectNode) {
            rectNode = d->sceneGraphContext()->createInternalRectangleNode();
            rectNode->setColor(color);
            rootTransformNode->appendChildNode(rectNode);
        }

        rectNode->setRect(QRectF(QPointF(0, y), QSizeF(m_progress * width(), implicitHeight())));
        rectNode->update();
    }

    return oldNode;
}

QQuickProgressAnimator::QQuickProgressAnimator(QObject *parent) :
    QQuickAnimator(parent)
{
    setDuration(animationDuration);
    setLoops(QQuickAnimator::Infinite);
}

QString QQuickProgressAnimator::propertyName() const
{
    return QString();
}

QQuickAnimatorJob *QQuickProgressAnimator::createJob() const
{
    return new QQuickProgressAnimatorJob;
}

QQuickProgressAnimatorJob::QQuickProgressAnimatorJob() :
    m_node(nullptr)
{
}

QQuickProgressAnimatorJob::~QQuickProgressAnimatorJob()
{
}

void QQuickProgressAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    QQuickAnimatorJob::initialize(controller);
    m_node = QQuickItemPrivate::get(m_target)->childContainerNode();
}

void QQuickProgressAnimatorJob::afterNodeSync()
{
    m_node = QQuickItemPrivate::get(m_target)->childContainerNode();
}

void QQuickProgressAnimatorJob::updateCurrentTime(int time)
{
    if (!m_node)
        return;

    QSGRectangleNode *rootRectNode = static_cast<QSGRectangleNode *>(m_node->firstChild());
    if (!rootRectNode)
        return;
    Q_ASSERT(rootRectNode->type() == QSGNode::GeometryNodeType);

    QSGTransformNode *rootTransformNode = static_cast<QSGTransformNode*>(rootRectNode->firstChild());
    Q_ASSERT(rootTransformNode->type() == QSGNode::TransformNodeType);

    QSGTransformNode *transformNode = static_cast<QSGTransformNode*>(rootTransformNode->firstChild());
    // This function can be called without the relevant nodes having been created yet,
    // which can happen if you set indeterminate to true at runtime.
    if (!transformNode || transformNode->type() != QSGNode::TransformNodeType)
        return;

    const qreal pixelsPerSecond = rootRectNode->rect().width();

    for (int i = 0; i < blocks; ++i) {
        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode*>(transformNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        QMatrix4x4 m;
        const qreal restX = blockRestX(i, rootRectNode->rect().width());
        const qreal timeInSeconds = time / 1000.0;

        if (time < secondPhaseStart) {
            // Move into the resting position for the first phase.
            QEasingCurve easingCurve(QEasingCurve::InQuad);
            const qreal easedCompletion = easingCurve.valueForProgress(time / qreal(secondPhaseStart));
            const qreal distance = pixelsPerSecond * (easedCompletion * (secondPhaseStart / 1000.0));
            const qreal position = blockStartX(i) + distance;
            const qreal destination = restX;
            m.translate(qMin(position, destination), 0);
        } else if (time < thirdPhaseStart) {
            // Stay in the same position for the second phase.
            m.translate(restX, 0);
        } else {
            // Move out of view for the third phase.
            const int thirdPhaseSubKickoff = (blockMovingSpacing / pixelsPerSecond) * 1000;
            const int subphase = (time - thirdPhaseStart) / thirdPhaseSubKickoff;
            // If we're not at this subphase yet, don't try to animate movement,
            // because it will be incorrect.
            if (subphase < i)
                return;

            const qreal timeSinceSecondPhase = timeInSeconds - (thirdPhaseStart / 1000.0);
            // We only want to start keeping track of time once our subphase has started,
            // otherwise we move too much because we account for time that has already elapsed.
            // For example, if we were 60 milliseconds into the third subphase:
            //
            //      0 ..... 1 ..... 2 ...
            //         100     100     60
            //
            // i == 0, timeSinceOurKickoff == 260
            // i == 1, timeSinceOurKickoff == 160
            // i == 2, timeSinceOurKickoff ==  60
            const qreal timeSinceOurKickoff = timeSinceSecondPhase - (thirdPhaseSubKickoff / 1000.0 * i);
            const qreal position = restX + (pixelsPerSecond * (timeSinceOurKickoff));
            const qreal destination = blockEndX(i, rootRectNode->rect().width());
            m.translate(qMin(position, destination), 0);
        }

        transformNode->setMatrix(m);
        rectNode->update();

        transformNode = static_cast<QSGTransformNode*>(transformNode->nextSibling());
    }
}

void QQuickProgressAnimatorJob::writeBack()
{
}

void QQuickProgressAnimatorJob::nodeWasDestroyed()
{
    m_node = nullptr;
}

QT_END_NAMESPACE
