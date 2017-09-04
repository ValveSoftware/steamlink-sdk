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

#include "qquickmaterialprogressbar_p.h"

#include <QtCore/qmath.h>
#include <QtCore/qeasingcurve.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuick/qsgrectanglenode.h>
#include <QtQuick/qsgimagenode.h>
#include <QtQuick/qquickwindow.h>
#include <QtQuickControls2/private/qquickanimatednode_p.h>

QT_BEGIN_NAMESPACE

static const int PauseDuration = 520;
static const int SlideDuration = 1240;
static const int TotalDuration = SlideDuration + PauseDuration;

class QQuickMaterialProgressBarNode : public QQuickAnimatedNode
{
public:
    QQuickMaterialProgressBarNode(QQuickMaterialProgressBar *item);

    void updateCurrentTime(int time) override;
    void sync(QQuickItem *item) override;

private:
    void moveNode(QSGTransformNode *node, const QRectF &geometry, qreal progress);

    bool m_indeterminate;
    QEasingCurve m_easing;
};

QQuickMaterialProgressBarNode::QQuickMaterialProgressBarNode(QQuickMaterialProgressBar *item)
    : QQuickAnimatedNode(item),
      m_indeterminate(false),
      m_easing(QEasingCurve::OutCubic)
{
    setLoopCount(Infinite);
    setDuration(TotalDuration);
}

void QQuickMaterialProgressBarNode::updateCurrentTime(int time)
{
    QSGRectangleNode *geometryNode = static_cast<QSGRectangleNode *>(firstChild());
    Q_ASSERT(geometryNode->type() == QSGNode::GeometryNodeType);
    const QRectF geometry = geometryNode->rect();

    QSGTransformNode *firstNode = static_cast<QSGTransformNode *>(geometryNode->firstChild());
    if (firstNode) {
        Q_ASSERT(firstNode->type() == QSGNode::TransformNodeType);

        const qreal progress = qMin<qreal>(1.0, static_cast<qreal>(time) / SlideDuration);
        moveNode(static_cast<QSGTransformNode *>(firstNode), geometry, progress);
    }

    QSGTransformNode *secondNode = static_cast<QSGTransformNode *>(geometryNode->lastChild());
    if (secondNode) {
        Q_ASSERT(secondNode->type() == QSGNode::TransformNodeType);

        const qreal progress = qMax<qreal>(0.0, static_cast<qreal>(time - PauseDuration) / SlideDuration);
        moveNode(static_cast<QSGTransformNode *>(secondNode), geometry, progress);
    }
}

void QQuickMaterialProgressBarNode::sync(QQuickItem *item)
{
    QQuickMaterialProgressBar *bar = static_cast<QQuickMaterialProgressBar *>(item);
    if (m_indeterminate != bar->isIndeterminate()) {
        m_indeterminate = bar->isIndeterminate();
        if (m_indeterminate)
            start();
        else
            stop();
    }

    QQuickItemPrivate *d = QQuickItemPrivate::get(item);

    QRectF bounds = item->boundingRect();
    bounds.setHeight(item->implicitHeight());
    bounds.moveTop((item->height() - bounds.height()) / 2.0);

    QSGRectangleNode *geometryNode = static_cast<QSGRectangleNode *>(firstChild());
    if (!geometryNode) {
        geometryNode = item->window()->createRectangleNode();
        geometryNode->setColor(Qt::transparent);
        appendChildNode(geometryNode);
    }
    geometryNode->setRect(bounds);

    const int count = m_indeterminate ? 2 : 1;
    const qreal w = m_indeterminate ? 0 : bar->progress() * item->width();
    const QRectF rect(0, bounds.y(), w, bounds.height());

    QSGNode *transformNode = geometryNode->firstChild();
    for (int i = 0; i < count; ++i) {
        if (!transformNode) {
            transformNode = new QSGTransformNode;
            geometryNode->appendChildNode(transformNode);

            QSGInternalRectangleNode *rectNode = d->sceneGraphContext()->createInternalRectangleNode();
            rectNode->setAntialiasing(true);
            transformNode->appendChildNode(rectNode);
        }
        Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);
        static_cast<QSGTransformNode *>(transformNode)->setMatrix(QMatrix4x4());

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(transformNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        rectNode->setRect(rect);
        rectNode->setColor(bar->color());
        rectNode->update();

        transformNode = transformNode->nextSibling();
    }

    while (transformNode) {
        QSGNode *nextSibling = transformNode->nextSibling();
        delete transformNode;
        transformNode = nextSibling;
    }
}

void QQuickMaterialProgressBarNode::moveNode(QSGTransformNode *transformNode, const QRectF &geometry, qreal progress)
{
    const qreal value = m_easing.valueForProgress(progress);
    const qreal x = value * geometry.width();

    QMatrix4x4 matrix;
    matrix.translate(x, 0);
    transformNode->setMatrix(matrix);

    QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(transformNode->firstChild());
    Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

    QRectF r = geometry;
    r.setWidth(value * (geometry.width() - x));
    rectNode->setRect(r);
    rectNode->update();
}

QQuickMaterialProgressBar::QQuickMaterialProgressBar(QQuickItem *parent)
    : QQuickItem(parent), m_color(Qt::black), m_progress(0.0), m_indeterminate(false)
{
    setFlag(ItemHasContents);
}

QColor QQuickMaterialProgressBar::color() const
{
    return m_color;
}

void QQuickMaterialProgressBar::setColor(const QColor &color)
{
    if (color == m_color)
        return;

    m_color = color;
    update();
}

qreal QQuickMaterialProgressBar::progress() const
{
    return m_progress;
}

void QQuickMaterialProgressBar::setProgress(qreal progress)
{
    if (progress == m_progress)
        return;

    m_progress = progress;
    update();
}

bool QQuickMaterialProgressBar::isIndeterminate() const
{
    return m_indeterminate;
}

void QQuickMaterialProgressBar::setIndeterminate(bool indeterminate)
{
    if (indeterminate == m_indeterminate)
        return;

    m_indeterminate = indeterminate;
    update();
}

void QQuickMaterialProgressBar::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged)
        update();
}

QSGNode *QQuickMaterialProgressBar::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QQuickMaterialProgressBarNode *node = static_cast<QQuickMaterialProgressBarNode *>(oldNode);
    if (isVisible() && width() > 0 && height() > 0) {
        if (!node)
            node = new QQuickMaterialProgressBarNode(this);
        node->sync(this);
    } else {
        delete node;
        node = nullptr;
    }
    return node;
}

QT_END_NAMESPACE
