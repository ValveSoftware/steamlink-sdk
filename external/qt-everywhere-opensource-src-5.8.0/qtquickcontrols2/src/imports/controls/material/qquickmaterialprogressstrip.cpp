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

#include "qquickmaterialprogressstrip_p.h"

#include <QtCore/qmath.h>
#include <QtCore/qeasingcurve.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickanimatorjob_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuick/qsgrectanglenode.h>
#include <QtQuick/qsgimagenode.h>

QT_BEGIN_NAMESPACE

static const int PauseDuration = 520;
static const int SlideDuration = 1240;
static const int TotalDuration = SlideDuration + PauseDuration;

class QQuickMaterialProgressStripAnimatorJob : public QQuickAnimatorJob
{
public:
    QQuickMaterialProgressStripAnimatorJob();

    void initialize(QQuickAnimatorController *controller) override;
    void updateCurrentTime(int time) override;
    void writeBack() override;
    void nodeWasDestroyed() override;
    void afterNodeSync() override;

    void moveNode(QSGTransformNode *node, const QRectF &geometry, qreal progress);

private:
    QSGNode *m_node;
};

QQuickMaterialProgressStripAnimatorJob::QQuickMaterialProgressStripAnimatorJob() : m_node(nullptr)
{
}

void QQuickMaterialProgressStripAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    QQuickAnimatorJob::initialize(controller);
    m_node = QQuickItemPrivate::get(m_target)->childContainerNode();
}

void QQuickMaterialProgressStripAnimatorJob::updateCurrentTime(int time)
{
    if (!m_node)
        return;

    QSGRectangleNode *geometryNode = static_cast<QSGRectangleNode *>(m_node->firstChild());
    Q_ASSERT(!geometryNode || geometryNode->type() == QSGNode::GeometryNodeType);
    if (!geometryNode)
        return;

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

void QQuickMaterialProgressStripAnimatorJob::writeBack()
{
}

void QQuickMaterialProgressStripAnimatorJob::nodeWasDestroyed()
{
    m_node = nullptr;
}

void QQuickMaterialProgressStripAnimatorJob::afterNodeSync()
{
    m_node = QQuickItemPrivate::get(m_target)->childContainerNode();
}

void QQuickMaterialProgressStripAnimatorJob::moveNode(QSGTransformNode *transformNode, const QRectF &geometry, qreal progress)
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

QQuickMaterialStripAnimator::QQuickMaterialStripAnimator(QObject *parent)
    : QQuickAnimator(parent)
{
    setLoops(Infinite);
    setDuration(TotalDuration);
    setEasing(QEasingCurve::OutCubic);
}

QString QQuickMaterialStripAnimator::propertyName() const
{
    return QString();
}

QQuickAnimatorJob *QQuickMaterialStripAnimator::createJob() const
{
    return new QQuickMaterialProgressStripAnimatorJob;
}

QQuickMaterialProgressStrip::QQuickMaterialProgressStrip(QQuickItem *parent)
    : QQuickItem(parent), m_color(Qt::black), m_progress(0.0), m_indeterminate(false)
{
    setFlag(ItemHasContents);
}

QColor QQuickMaterialProgressStrip::color() const
{
    return m_color;
}

void QQuickMaterialProgressStrip::setColor(const QColor &color)
{
    if (color == m_color)
        return;

    m_color = color;
    update();
}

qreal QQuickMaterialProgressStrip::progress() const
{
    return m_progress;
}

void QQuickMaterialProgressStrip::setProgress(qreal progress)
{
    if (progress == m_progress)
        return;

    m_progress = progress;
    update();
}

bool QQuickMaterialProgressStrip::isIndeterminate() const
{
    return m_indeterminate;
}

void QQuickMaterialProgressStrip::setIndeterminate(bool indeterminate)
{
    if (indeterminate == m_indeterminate)
        return;

    m_indeterminate = indeterminate;
    update();
}

QSGNode *QQuickMaterialProgressStrip::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QQuickItemPrivate *d = QQuickItemPrivate::get(this);

    QRectF bounds = boundingRect();
    bounds.setHeight(implicitHeight());
    bounds.moveTop((height() - bounds.height()) / 2.0);

    if (!oldNode) {
        oldNode = window()->createRectangleNode();
        static_cast<QSGRectangleNode *>(oldNode)->setColor(Qt::transparent);
    }
    static_cast<QSGRectangleNode *>(oldNode)->setRect(bounds);

    const int count = m_indeterminate ? 2 : 1;
    const qreal w = m_indeterminate ? 0 : m_progress * width();
    const QRectF rect(0, bounds.y(), w, bounds.height());

    QSGNode *transformNode = oldNode->firstChild();
    for (int i = 0; i < count; ++i) {
        if (!transformNode) {
            transformNode = new QSGTransformNode;
            oldNode->appendChildNode(transformNode);

            QSGInternalRectangleNode *rectNode = d->sceneGraphContext()->createInternalRectangleNode();
            rectNode->setAntialiasing(true);
            transformNode->appendChildNode(rectNode);
        }
        Q_ASSERT(transformNode->type() == QSGNode::TransformNodeType);
        static_cast<QSGTransformNode *>(transformNode)->setMatrix(QMatrix4x4());

        QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(transformNode->firstChild());
        Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

        rectNode->setRect(rect);
        rectNode->setColor(m_color);
        rectNode->update();

        transformNode = transformNode->nextSibling();
    }

    while (transformNode) {
        QSGNode *nextSibling = transformNode->nextSibling();
        delete transformNode;
        transformNode = nextSibling;
    }

    return oldNode;
}

QT_END_NAMESPACE
