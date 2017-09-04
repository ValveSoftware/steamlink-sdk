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

#include "qquickdefaultprogressbar_p.h"

#include <QtCore/qeasingcurve.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuickControls2/private/qquickanimatednode_p.h>

QT_BEGIN_NAMESPACE

static const int Blocks = 4;
static const int BlockWidth = 16;
static const int BlockRestingSpacing = 4;
static const int BlockMovingSpacing = 48;
static const int BlockSpan = Blocks * (BlockWidth + BlockRestingSpacing) - BlockRestingSpacing;
static const int TotalDuration = 4000;
static const int SecondPhaseStart = TotalDuration * 0.4;
static const int ThirdPhaseStart = TotalDuration * 0.6;
static const QRgb FillColor = 0x353637;

static inline qreal blockStartX(int blockIndex)
{
    return ((blockIndex + 1) * -BlockWidth) - (blockIndex * BlockMovingSpacing);
}

static inline qreal blockRestX(int blockIndex, qreal availableWidth)
{
    const qreal spanRightEdgePos = availableWidth / 2 + BlockSpan / 2;
    return spanRightEdgePos - (blockIndex + 1) * BlockWidth - (blockIndex * BlockRestingSpacing);
}

static inline qreal blockEndX(int blockIndex, qreal availableWidth)
{
    return availableWidth - blockStartX(Blocks - 1 - blockIndex) - BlockWidth;
}

class QQuickDefaultProgressBarNode : public QQuickAnimatedNode
{
public:
    QQuickDefaultProgressBarNode(QQuickDefaultProgressBar *item);

    void updateCurrentTime(int time) override;
    void sync(QQuickItem *item) override;

private:
    bool m_indeterminate;
    qreal m_pixelsPerSecond;
};

QQuickDefaultProgressBarNode::QQuickDefaultProgressBarNode(QQuickDefaultProgressBar *item)
    : QQuickAnimatedNode(item),
      m_indeterminate(false),
      m_pixelsPerSecond(item->width())
{
    setLoopCount(Infinite);
    setDuration(TotalDuration);
}

void QQuickDefaultProgressBarNode::updateCurrentTime(int time)
{
    QSGTransformNode *transformNode = static_cast<QSGTransformNode*>(firstChild());
    for (int i = 0; i < Blocks; ++i) {
        Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);

        QMatrix4x4 m;
        const qreal restX = blockRestX(i, m_pixelsPerSecond);
        const qreal timeInSeconds = time / 1000.0;

        if (time < SecondPhaseStart) {
            // Move into the resting position for the first phase.
            QEasingCurve easingCurve(QEasingCurve::InQuad);
            const qreal easedCompletion = easingCurve.valueForProgress(time / qreal(SecondPhaseStart));
            const qreal distance = m_pixelsPerSecond * (easedCompletion * (SecondPhaseStart / 1000.0));
            const qreal position = blockStartX(i) + distance;
            const qreal destination = restX;
            m.translate(qMin(position, destination), 0);
        } else if (time < ThirdPhaseStart) {
            // Stay in the same position for the second phase.
            m.translate(restX, 0);
        } else {
            // Move out of view for the third phase.
            const int thirdPhaseSubKickoff = (BlockMovingSpacing / m_pixelsPerSecond) * 1000;
            const int subphase = (time - ThirdPhaseStart) / thirdPhaseSubKickoff;
            // If we're not at this subphase yet, don't try to animate movement,
            // because it will be incorrect.
            if (subphase < i)
                return;

            const qreal timeSinceSecondPhase = timeInSeconds - (ThirdPhaseStart / 1000.0);
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
            const qreal position = restX + (m_pixelsPerSecond * (timeSinceOurKickoff));
            const qreal destination = blockEndX(i, m_pixelsPerSecond);
            m.translate(qMin(position, destination), 0);
        }

        transformNode->setMatrix(m);

        transformNode = static_cast<QSGTransformNode*>(transformNode->nextSibling());
    }
}

void QQuickDefaultProgressBarNode::sync(QQuickItem *item)
{
    QQuickDefaultProgressBar *bar = static_cast<QQuickDefaultProgressBar *>(item);
    if (m_indeterminate != bar->isIndeterminate()) {
        m_indeterminate = bar->isIndeterminate();
        if (m_indeterminate)
            start();
        else
            stop();
    }
    m_pixelsPerSecond = item->width();

    QQuickItemPrivate *d = QQuickItemPrivate::get(item);

    QMatrix4x4 m;
    m.translate(0, (item->height() - item->implicitHeight()) / 2);
    setMatrix(m);

    if (m_indeterminate) {
        if (childCount() != Blocks) {
            // This was previously a regular progress bar; remove the old nodes.
            removeAllChildNodes();
        }

        QSGTransformNode *transformNode = static_cast<QSGTransformNode*>(firstChild());
        for (int i = 0; i < Blocks; ++i) {
            if (!transformNode) {
                transformNode = new QSGTransformNode;
                appendChildNode(transformNode);
            }

            QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode*>(transformNode->firstChild());
            if (!rectNode) {
                rectNode = d->sceneGraphContext()->createInternalRectangleNode();
                rectNode->setColor(FillColor);
                transformNode->appendChildNode(rectNode);
            }

            QMatrix4x4 m;
            m.translate(blockStartX(i), 0);
            transformNode->setMatrix(m);

            rectNode->setRect(QRectF(QPointF(0, 0), QSizeF(BlockWidth, item->implicitHeight())));
            rectNode->update();

            transformNode = static_cast<QSGTransformNode *>(transformNode->nextSibling());
        }
    } else {
        if (childCount() > 1) {
            // This was previously an indeterminate progress bar; remove the old nodes.
            removeAllChildNodes();
        }

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(firstChild());
        if (!rectNode) {
            rectNode = d->sceneGraphContext()->createInternalRectangleNode();
            rectNode->setColor(FillColor);
            appendChildNode(rectNode);
        }

        rectNode->setRect(QRectF(QPointF(0, 0), QSizeF(bar->progress() * item->width(), item->implicitHeight())));
        rectNode->update();
    }
}

QQuickDefaultProgressBar::QQuickDefaultProgressBar(QQuickItem *parent) :
    QQuickItem(parent),
    m_progress(0),
    m_indeterminate(false)
{
    setFlag(ItemHasContents);
}

qreal QQuickDefaultProgressBar::progress() const
{
    return m_progress;
}

void QQuickDefaultProgressBar::setProgress(qreal progress)
{
    if (progress == m_progress)
        return;

    m_progress = progress;
    update();
    emit progressChanged();
}

bool QQuickDefaultProgressBar::isIndeterminate() const
{
    return m_indeterminate;
}

void QQuickDefaultProgressBar::setIndeterminate(bool indeterminate)
{
    if (indeterminate == m_indeterminate)
        return;

    m_indeterminate = indeterminate;
    setClip(m_indeterminate);
    update();
    emit indeterminateChanged();
}

void QQuickDefaultProgressBar::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged)
        update();
}

QSGNode *QQuickDefaultProgressBar::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QQuickDefaultProgressBarNode *node = static_cast<QQuickDefaultProgressBarNode *>(oldNode);
    if (isVisible() && width() > 0 && height() > 0) {
        if (!node)
            node = new QQuickDefaultProgressBarNode(this);
        node->sync(this);
    } else {
        delete node;
        node = nullptr;
    }
    return node;
}

QT_END_NAMESPACE
