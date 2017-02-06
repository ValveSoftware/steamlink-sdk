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

#include "qquickmaterialripple_p.h"

#include <QtCore/qmath.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickanimator_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquickanimatorjob_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuickTemplates2/private/qquickabstractbutton_p.h>
#include <QtQuickTemplates2/private/qquickabstractbutton_p_p.h>

QT_BEGIN_NAMESPACE

namespace {
    enum WavePhase { WaveEnter, WaveExit };
}

static const int RIPPLE_ENTER_DELAY = 80;
static const int OPACITY_ENTER_DURATION_FAST = 120;
static const int WAVE_OPACITY_DECAY_DURATION = 333;
static const qreal WAVE_TOUCH_UP_ACCELERATION = 3400.0;
static const qreal WAVE_TOUCH_DOWN_ACCELERATION = 1024.0;

class QQuickMaterialRippleAnimatorJob : public QQuickAnimatorJob
{
public:
    QQuickMaterialRippleAnimatorJob(WavePhase phase, const QPointF &anchor, const QRectF &bounds);

    void initialize(QQuickAnimatorController *controller) override;
    void updateCurrentTime(int time) override;
    void writeBack() override;
    void nodeWasDestroyed() override;
    void afterNodeSync() override;

private:
    qreal m_diameter;
    WavePhase m_phase;
    QRectF m_bounds;
    QPointF m_anchor;
    QSGTransformNode *m_itemNode;
    QSGOpacityNode *m_opacityNode;
    QSGInternalRectangleNode *m_rectNode;
};

QQuickMaterialRippleAnimatorJob::QQuickMaterialRippleAnimatorJob(WavePhase phase, const QPointF &anchor, const QRectF &bounds)
    : m_diameter(qSqrt(bounds.width() * bounds.width() + bounds.height() * bounds.height())),
      m_phase(phase),
      m_bounds(bounds),
      m_anchor(anchor),
      m_itemNode(nullptr),
      m_opacityNode(nullptr),
      m_rectNode(nullptr)
{
}

void QQuickMaterialRippleAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    QQuickAnimatorJob::initialize(controller);
    afterNodeSync();
}

void QQuickMaterialRippleAnimatorJob::updateCurrentTime(int time)
{
    if (!m_itemNode || !m_rectNode)
        return;

    qreal duration = 0;
    if (m_phase == WaveEnter)
        duration = QQuickAnimatorJob::duration();
    else
        duration = 1000.0 * qSqrt((m_diameter - m_from) / 2.0 / (WAVE_TOUCH_UP_ACCELERATION + WAVE_TOUCH_DOWN_ACCELERATION));

    qreal p = 1.0;
    if (!qFuzzyIsNull(duration) && time < duration)
        p = time / duration;

    m_value = m_from + (m_to - m_from) * p;
    p = m_value / m_diameter;

    const qreal dx = (1.0 - p) * (m_anchor.x() - m_bounds.width() / 2);
    const qreal dy = (1.0 - p) * (m_anchor.y() - m_bounds.height() / 2);

    m_rectNode->setRect(QRectF(0, 0, m_value, m_value));
    m_rectNode->setRadius(m_value / 2);
    m_rectNode->update();

    QMatrix4x4 m;
    m.translate((m_bounds.width() - m_value) / 2 + dx,
                (m_bounds.height() - m_value) / 2 + dy);
    m_itemNode->setMatrix(m);

    if (m_opacityNode) {
        qreal opacity = 1.0;
        if (m_phase == WaveExit)
            opacity -= static_cast<qreal>(time) / WAVE_OPACITY_DECAY_DURATION;
        m_opacityNode->setOpacity(opacity);
    }
}

void QQuickMaterialRippleAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setSize(QSizeF(m_value, m_value));
    if (m_phase == WaveExit)
        m_target->deleteLater();
}

void QQuickMaterialRippleAnimatorJob::nodeWasDestroyed()
{
    m_itemNode = nullptr;
    m_opacityNode = nullptr;
    m_rectNode = nullptr;
}

void QQuickMaterialRippleAnimatorJob::afterNodeSync()
{
    m_itemNode = QQuickItemPrivate::get(m_target)->itemNode();
    m_opacityNode = QQuickItemPrivate::get(m_target)->opacityNode();
    m_rectNode = static_cast<QSGInternalRectangleNode *>(QQuickItemPrivate::get(m_target)->childContainerNode()->firstChild());
}

class QQuickMaterialRippleAnimator : public QQuickAnimator
{
public:
    QQuickMaterialRippleAnimator(const QPointF &anchor, const QRectF &bounds, QObject *parent = nullptr);

    WavePhase phase() const;
    void setPhase(WavePhase phase);

protected:
    QString propertyName() const override;
    QQuickAnimatorJob *createJob() const override;

private:
    QPointF m_anchor;
    QRectF m_bounds;
    WavePhase m_phase;
};

QQuickMaterialRippleAnimator::QQuickMaterialRippleAnimator(const QPointF &anchor, const QRectF &bounds, QObject *parent)
    : QQuickAnimator(parent), m_anchor(anchor), m_bounds(bounds), m_phase(WaveEnter)
{
}

WavePhase QQuickMaterialRippleAnimator::phase() const
{
    return m_phase;
}

void QQuickMaterialRippleAnimator::setPhase(WavePhase phase)
{
    if (m_phase == phase)
        return;

    m_phase = phase;
}

QString QQuickMaterialRippleAnimator::propertyName() const
{
    return QString();
}

QQuickAnimatorJob *QQuickMaterialRippleAnimator::createJob() const
{
    return new QQuickMaterialRippleAnimatorJob(m_phase, m_anchor, m_bounds);
}

QQuickMaterialRipple::QQuickMaterialRipple(QQuickItem *parent)
    : QQuickItem(parent), m_active(false), m_pressed(false), m_enterDelay(0), m_trigger(Press), m_clipRadius(0.0), m_anchor(nullptr), m_opacityAnimator(nullptr)
{
    setOpacity(0.0);
    setFlag(ItemHasContents);
}

bool QQuickMaterialRipple::isActive() const
{
    return m_active;
}

void QQuickMaterialRipple::setActive(bool active)
{
    if (active == m_active)
        return;

    m_active = active;

    if (!m_opacityAnimator) {
        m_opacityAnimator = new QQuickOpacityAnimator(this);
        m_opacityAnimator->setTargetItem(this);
    }
    m_opacityAnimator->setDuration(active ? OPACITY_ENTER_DURATION_FAST : WAVE_OPACITY_DECAY_DURATION);

    const int time = m_opacityAnimator->currentTime();
    m_opacityAnimator->stop();
    m_opacityAnimator->setFrom(opacity());
    m_opacityAnimator->setTo(active ? 1.0 : 0.0);
    m_opacityAnimator->setCurrentTime(time);
    m_opacityAnimator->start();
}

QColor QQuickMaterialRipple::color() const
{
    return m_color;
}

void QQuickMaterialRipple::setColor(const QColor &color)
{
    if (m_color == color)
        return;

    m_color = color;
    update();
}

qreal QQuickMaterialRipple::clipRadius() const
{
    return m_clipRadius;
}

void QQuickMaterialRipple::setClipRadius(qreal radius)
{
    if (qFuzzyCompare(m_clipRadius, radius))
        return;

    m_clipRadius = radius;
    setClip(!qFuzzyIsNull(radius));
    update();
}

bool QQuickMaterialRipple::isPressed() const
{
    return m_pressed;
}

void QQuickMaterialRipple::setPressed(bool pressed)
{
    if (pressed == m_pressed)
        return;

    m_pressed = pressed;

    if (!isEnabled())
        return;

    if (pressed) {
        if (m_trigger == Press)
            prepareWave();
        else
            exitWave();
    } else {
        if (m_trigger == Release)
            enterWave();
        else
            exitWave();
    }
}

QQuickMaterialRipple::Trigger QQuickMaterialRipple::trigger() const
{
    return m_trigger;
}

void QQuickMaterialRipple::setTrigger(Trigger trigger)
{
    m_trigger = trigger;
}

QPointF QQuickMaterialRipple::anchorPoint() const
{
    const QRectF bounds = boundingRect();
    const QPointF center = bounds.center();
    if (!m_anchor)
        return center;

    QPointF anchorPoint = bounds.center();
    if (QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton *>(m_anchor))
        anchorPoint = QQuickAbstractButtonPrivate::get(button)->pressPoint;
    anchorPoint = mapFromItem(m_anchor, anchorPoint);

    // calculate whether the anchor point is within the ripple circle bounds,
    // that is, whether waves should start expanding from the anchor point
    const qreal r = qSqrt(bounds.width() * bounds.width() + bounds.height() * bounds.height()) / 2;
    if (QLineF(center, anchorPoint).length() < r)
        return anchorPoint;

    // if the anchor point is outside the ripple circle bounds, start expanding
    // from the intersection point of the ripple circle and a line from its center
    // to the the anchor point
    const qreal p = qAtan2(anchorPoint.y() - center.y(), anchorPoint.x() - center.x());
    return QPointF(center.x() + r * qCos(p), center.y() + r * qSin(p));
}

QQuickItem *QQuickMaterialRipple::anchor() const
{
    return m_anchor;
}

void QQuickMaterialRipple::setAnchor(QQuickItem *item)
{
    m_anchor = item;
}

void QQuickMaterialRipple::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);

    if (change == ItemChildRemovedChange) {
        QQuickMaterialRippleAnimator *animator = data.item->findChild<QQuickMaterialRippleAnimator *>();
        if (animator)
            m_rippleAnimators.removeOne(animator);
    }
}

QSGNode *QQuickMaterialRipple::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QQuickItemPrivate *d = QQuickItemPrivate::get(this);
    QQuickDefaultClipNode *clipNode = d->clipNode();
    if (clipNode) {
        // TODO: QTBUG-51894
        // clipNode->setRadius(m_clipRadius);
        clipNode->setRect(boundingRect());
        clipNode->update();
    }

    const qreal w = width();
    const qreal h = height();
    const qreal sz = qSqrt(w * w + h * h);

    QSGTransformNode *transformNode = static_cast<QSGTransformNode *>(oldNode);
    if (!transformNode)
        transformNode = new QSGTransformNode;

    QSGInternalRectangleNode *rectNode = static_cast<QSGInternalRectangleNode *>(transformNode->firstChild());
    if (!rectNode) {
        rectNode = d->sceneGraphContext()->createInternalRectangleNode();
        rectNode->setAntialiasing(true);
        transformNode->appendChildNode(rectNode);
    }

    QMatrix4x4 matrix;
    if (qFuzzyIsNull(m_clipRadius)) {
        matrix.translate((w - sz) / 2, (h - sz) / 2);
        rectNode->setRect(QRectF(0, 0, sz, sz));
        rectNode->setRadius(sz / 2);
    } else {
        rectNode->setRect(QRectF(0, 0, w, h));
        rectNode->setRadius(m_clipRadius);
    }
    transformNode->setMatrix(matrix);
    rectNode->setColor(m_color);
    rectNode->update();

    return transformNode;
}

void QQuickMaterialRipple::timerEvent(QTimerEvent *event)
{
    QQuickItem::timerEvent(event);

    if (event->timerId() == m_enterDelay)
        enterWave();
}

void QQuickMaterialRipple::prepareWave()
{
    if (m_enterDelay <= 0)
        m_enterDelay = startTimer(RIPPLE_ENTER_DELAY);
}

void QQuickMaterialRipple::enterWave()
{
    if (m_enterDelay > 0) {
        killTimer(m_enterDelay);
        m_enterDelay = 0;
    }

    const qreal w = width();
    const qreal h = height();
    const qreal sz = qSqrt(w * w + h * h);

    QQuickRectangle *wave = new QQuickRectangle(this);
    wave->setPosition(QPointF((w - sz) / 2, (h - sz) / 2));
    wave->setSize(QSizeF(sz, sz));
    wave->setRadius(sz / 2);
    wave->setColor(color());
    wave->setOpacity(0.0);

    QQuickMaterialRippleAnimator *animator = new QQuickMaterialRippleAnimator(anchorPoint(), boundingRect(), wave);
    animator->setDuration(qRound(1000.0 * qSqrt(sz / 2.0 / WAVE_TOUCH_DOWN_ACCELERATION)));
    animator->setTargetItem(wave);
    animator->setTo(sz);
    animator->start();
    m_rippleAnimators += animator;
}

void QQuickMaterialRipple::exitWave()
{
    if (m_enterDelay > 0) {
        killTimer(m_enterDelay);
        m_enterDelay = 0;
    }

    for (QQuickMaterialRippleAnimator *animator : m_rippleAnimators) {
        if (animator->phase() == WaveEnter) {
            animator->stop(); // -> writeBack() -> setSize()
            if (QQuickItem *wave = animator->targetItem())
                animator->setFrom(wave->width());
            animator->setDuration(WAVE_OPACITY_DECAY_DURATION);
            animator->setPhase(WaveExit);
            animator->restart();
        }
    }
}

QT_END_NAMESPACE
