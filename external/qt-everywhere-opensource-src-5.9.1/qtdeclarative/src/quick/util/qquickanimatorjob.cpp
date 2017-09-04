/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Gunnar Sletta <gunnar@sletta.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickanimatorcontroller_p.h"
#include "qquickanimatorjob_p.h"
#include "qquickanimator_p.h"
#include "qquickanimator_p_p.h"
#include <private/qquickwindow_p.h>
#include <private/qquickitem_p.h>
#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
# include <private/qquickopenglshadereffectnode_p.h>
# include <private/qquickopenglshadereffect_p.h>
# include <private/qquickshadereffect_p.h>
#endif
#include <private/qanimationgroupjob_p.h>

#include <qcoreapplication.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

struct QQuickTransformAnimatorHelperStore
{
    QHash<QQuickItem *, QQuickTransformAnimatorJob::Helper *> store;
    QMutex mutex;

    QQuickTransformAnimatorJob::Helper *acquire(QQuickItem *item) {
        mutex.lock();
        QQuickTransformAnimatorJob::Helper *helper = store.value(item);
        if (!helper) {
            helper = new QQuickTransformAnimatorJob::Helper();
            helper->item = item;
            store[item] = helper;
        } else {
            ++helper->ref;
        }
        mutex.unlock();
        return helper;
    }

    void release(QQuickTransformAnimatorJob::Helper *helper) {
        mutex.lock();
        if (--helper->ref == 0) {
            store.remove(helper->item);
            delete helper;
        }
        mutex.unlock();
    }
};
Q_GLOBAL_STATIC(QQuickTransformAnimatorHelperStore, qquick_transform_animatorjob_helper_store);

QQuickAnimatorProxyJob::QQuickAnimatorProxyJob(QAbstractAnimationJob *job, QObject *item)
    : m_controller(nullptr)
    , m_internalState(State_Stopped)
{
    m_job.reset(job);

    m_isRenderThreadProxy = true;
    m_animation = qobject_cast<QQuickAbstractAnimation *>(item);

    setLoopCount(job->loopCount());

    // Instead of setting duration to job->duration() we need to set it to -1 so that
    // it runs as long as the job is running on the render thread. If we gave it
    // an explicit duration, it would be stopped, potentially stopping the RT animation
    // prematurely.
    // This means that the animation driver will tick on the GUI thread as long
    // as the animation is running on the render thread, but this overhead will
    // be negligiblie compared to animating and re-rendering the scene on the render thread.
    m_duration = -1;

    QObject *ctx = findAnimationContext(m_animation);
    if (!ctx) {
        qWarning("QtQuick: unable to find animation context for RT animation...");
        return;
    }

    QQuickWindow *window = qobject_cast<QQuickWindow *>(ctx);
    if (window) {
        setWindow(window);
    } else {
        QQuickItem *item = qobject_cast<QQuickItem *>(ctx);
        if (item->window())
            setWindow(item->window());
        connect(item, &QQuickItem::windowChanged, this, &QQuickAnimatorProxyJob::windowChanged);
    }
}

QQuickAnimatorProxyJob::~QQuickAnimatorProxyJob()
{
    if (m_job && m_controller)
        m_controller->cancel(m_job);
    m_job.reset();
}

QObject *QQuickAnimatorProxyJob::findAnimationContext(QQuickAbstractAnimation *a)
{
    QObject *p = a->parent();
    while (p != nullptr && qobject_cast<QQuickWindow *>(p) == nullptr && qobject_cast<QQuickItem *>(p) == nullptr)
        p = p->parent();
    return p;
}

void QQuickAnimatorProxyJob::updateCurrentTime(int)
{
    if (m_internalState != State_Running)
        return;

    // A proxy which is being ticked should be associated with a window, (see
    // setWindow() below). If we get here when there is no more controller we
    // have a problem.
    Q_ASSERT(m_controller);

    // We do a simple check here to see if the animator has run and stopped on
    // the render thread. isPendingStart() will perform a check against jobs
    // that have been scheduled for start, but that will not yet have entered
    // the actual running state.
    // Secondly, we make an unprotected read of the job's state to figure out
    // if it is running, but this is ok, since we're only reading the state
    // and if the render thread should happen to be writing it concurrently,
    // we might get the wrong value for this update,  but then we'll simply
    // pick it up on the next iterationm when the job is stopped and render
    // thread is no longer using it.
    if (!m_controller->isPendingStart(m_job)
        && !m_job->isRunning()) {
        stop();
    }
}

void QQuickAnimatorProxyJob::updateState(QAbstractAnimationJob::State newState, QAbstractAnimationJob::State)
{
    if (m_state == Running) {
        m_internalState = State_Starting;
        if (m_controller) {
            m_internalState = State_Running;
            m_controller->start(m_job);
        }

    } else if (newState == Stopped) {
        m_internalState = State_Stopped;
        if (m_controller) {
            syncBackCurrentValues();
            m_controller->cancel(m_job);
        }
    }
}

void QQuickAnimatorProxyJob::debugAnimation(QDebug d) const
{
    d << "QuickAnimatorProxyJob("<< hex << (const void *) this << dec
      << "state:" << state() << "duration:" << duration()
      << "proxying: (" << job() << ')';
}

void QQuickAnimatorProxyJob::windowChanged(QQuickWindow *window)
{
    setWindow(window);
}

void QQuickAnimatorProxyJob::setWindow(QQuickWindow *window)
{
    if (!window) {
        if (m_job && m_controller)
            m_controller->cancel(m_job);
        m_controller = nullptr;
        stop();

    } else if (!m_controller && m_job) {
        m_controller = QQuickWindowPrivate::get(window)->animationController;
        if (window->isSceneGraphInitialized())
            readyToAnimate();
        else
            connect(window, &QQuickWindow::sceneGraphInitialized, this, &QQuickAnimatorProxyJob::sceneGraphInitialized);
    }
}

void QQuickAnimatorProxyJob::sceneGraphInitialized()
{
    disconnect(m_controller->window(), &QQuickWindow::sceneGraphInitialized, this, &QQuickAnimatorProxyJob::sceneGraphInitialized);
    readyToAnimate();
}

void QQuickAnimatorProxyJob::readyToAnimate()
{
    Q_ASSERT(m_controller);
    if (m_internalState == State_Starting) {
        m_internalState = State_Running;
        m_controller->start(m_job);
    }
}

static void qquick_syncback_helper(QAbstractAnimationJob *job)
{
    if (job->isRenderThreadJob()) {
        static_cast<QQuickAnimatorJob *>(job)->writeBack();

    } else if (job->isGroup()) {
        QAnimationGroupJob *g = static_cast<QAnimationGroupJob *>(job);
        for (QAbstractAnimationJob *a = g->firstChild(); a; a = a->nextSibling())
            qquick_syncback_helper(a);
    }

}

void QQuickAnimatorProxyJob::syncBackCurrentValues()
{
    if (m_job)
        qquick_syncback_helper(m_job.data());
}

QQuickAnimatorJob::QQuickAnimatorJob()
    : m_target(0)
    , m_controller(0)
    , m_from(0)
    , m_to(0)
    , m_value(0)
    , m_duration(0)
    , m_isTransform(false)
    , m_isUniform(false)
{
    m_isRenderThreadJob = true;
}

void QQuickAnimatorJob::debugAnimation(QDebug d) const
{
    d << "QuickAnimatorJob(" << hex << (const void *) this << dec
      << ") state:" << state() << "duration:" << duration()
      << "target:" << m_target << "value:" << m_value;
}

qreal QQuickAnimatorJob::progress(int time) const
{
    return m_easing.valueForProgress((m_duration == 0) ? qreal(1) : qreal(time) / qreal(m_duration));
}

qreal QQuickAnimatorJob::value() const
{
    qreal value = m_to;
    if (m_controller) {
        m_controller->lock();
        value = m_value;
        m_controller->unlock();
    }
    return value;
}

void QQuickAnimatorJob::setTarget(QQuickItem *target)
{
    m_target = target;
}

void QQuickAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    m_controller = controller;
}

QQuickTransformAnimatorJob::QQuickTransformAnimatorJob()
    : m_helper(nullptr)
{
    m_isTransform = true;
}

QQuickTransformAnimatorJob::~QQuickTransformAnimatorJob()
{
    if (m_helper)
        qquick_transform_animatorjob_helper_store()->release(m_helper);
}

void QQuickTransformAnimatorJob::setTarget(QQuickItem *item)
{
    // In the extremely unlikely event that the target of an animator has been
    // changed into a new item that sits in the exact same pointer address, we
    // want to force syncing it again.
    if (m_helper && m_target)
        m_helper->wasSynced = false;
    QQuickAnimatorJob::setTarget(item);
}

void QQuickTransformAnimatorJob::preSync()
{
    // If the target has changed or become null, release and reset the helper
    if (m_helper && (m_helper->item != m_target || !m_target)) {
        qquick_transform_animatorjob_helper_store()->release(m_helper);
        m_helper = nullptr;
    }

    if (!m_target)
        return;

    if (!m_helper) {
        m_helper = qquick_transform_animatorjob_helper_store()->acquire(m_target);

        // This is a bit superfluous, but it ends up being simpler than the
        // alternative.  When an item happens to land on the same address as a
        // previous item, that helper might not have been fully cleaned up by
        // the time it gets taken back into use. As an alternative to storing
        // connections to each and every item's QObject::destroyed() and
        // having to clean those up afterwards, we simply sync all helpers on
        // the first run. The sync is only done once for the run of an
        // animation and it is a fairly light function (compared to storing
        // potentially thousands of connections and managing their lifetime.
        m_helper->wasSynced = false;
    }

    m_helper->sync();
}

void QQuickTransformAnimatorJob::postSync()
{
    Q_ASSERT((m_helper != nullptr) == (m_target != nullptr)); // If there is a target, there should also be a helper, ref: preSync
    Q_ASSERT(!m_helper || m_helper->item == m_target); // If there is a helper, it should point to our target

    if (!m_target || !m_helper) {
        invalidate();
        return;
    }

    QQuickItemPrivate *d = QQuickItemPrivate::get(m_target);
#if QT_CONFIG(quick_shadereffect)
    if (d->extra.isAllocated()
            && d->extra->layer
            && d->extra->layer->enabled()) {
        d = QQuickItemPrivate::get(d->extra->layer->m_effectSource);
    }
#endif
    m_helper->node = d->itemNode();
}

void QQuickTransformAnimatorJob::invalidate()
{
    if (m_helper)
        m_helper->node = nullptr;
}

void QQuickTransformAnimatorJob::Helper::sync()
{
    const quint32 mask = QQuickItemPrivate::Position
            | QQuickItemPrivate::BasicTransform
            | QQuickItemPrivate::TransformOrigin
            | QQuickItemPrivate::Size;

    QQuickItemPrivate *d = QQuickItemPrivate::get(item);
#if QT_CONFIG(quick_shadereffect)
    if (d->extra.isAllocated()
            && d->extra->layer
            && d->extra->layer->enabled()) {
        d = QQuickItemPrivate::get(d->extra->layer->m_effectSource);
    }
#endif

    quint32 dirty = mask & d->dirtyAttributes;

    if (!wasSynced) {
        dirty = 0xffffffffu;
        wasSynced = true;
    }

    if (dirty == 0)
        return;

    node = d->itemNode();

    if (dirty & QQuickItemPrivate::Position) {
        dx = item->x();
        dy = item->y();
    }

    if (dirty & QQuickItemPrivate::BasicTransform) {
        scale = item->scale();
        rotation = item->rotation();
    }

    if (dirty & (QQuickItemPrivate::TransformOrigin | QQuickItemPrivate::Size)) {
        QPointF o = item->transformOriginPoint();
        ox = o.x();
        oy = o.y();
    }
}

void QQuickTransformAnimatorJob::Helper::commit()
{
    if (!wasChanged || !node)
        return;

    QMatrix4x4 m;
    m.translate(dx, dy);
    m.translate(ox, oy);
    m.scale(scale);
    m.rotate(rotation, 0, 0, 1);
    m.translate(-ox, -oy);
    node->setMatrix(m);

    wasChanged = false;
}

void QQuickTransformAnimatorJob::commit()
{
    if (m_helper)
        m_helper->commit();
}

void QQuickXAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setX(value());
}

void QQuickXAnimatorJob::updateCurrentTime(int time)
{
#if QT_CONFIG(opengl)
    Q_ASSERT(!m_controller || !m_controller->m_window->openglContext() || m_controller->m_window->openglContext()->thread() == QThread::currentThread());
#endif
    if (!m_helper)
        return;

    m_value = m_from + (m_to - m_from) * progress(time);
    m_helper->dx = m_value;
    m_helper->wasChanged = true;
}

void QQuickYAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setY(value());
}

void QQuickYAnimatorJob::updateCurrentTime(int time)
{
#if QT_CONFIG(opengl)
    Q_ASSERT(!m_controller || !m_controller->m_window->openglContext() || m_controller->m_window->openglContext()->thread() == QThread::currentThread());
#endif
    if (!m_helper)
        return;

    m_value = m_from + (m_to - m_from) * progress(time);
    m_helper->dy = m_value;
    m_helper->wasChanged = true;
}

void QQuickScaleAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setScale(value());
}

void QQuickScaleAnimatorJob::updateCurrentTime(int time)
{
#if QT_CONFIG(opengl)
    Q_ASSERT(!m_controller || !m_controller->m_window->openglContext() || m_controller->m_window->openglContext()->thread() == QThread::currentThread());
#endif
    if (!m_helper)
        return;

    m_value = m_from + (m_to - m_from) * progress(time);
    m_helper->scale = m_value;
    m_helper->wasChanged = true;
}


QQuickRotationAnimatorJob::QQuickRotationAnimatorJob()
    : m_direction(QQuickRotationAnimator::Numerical)
{
}

extern QVariant _q_interpolateShortestRotation(qreal &f, qreal &t, qreal progress);
extern QVariant _q_interpolateClockwiseRotation(qreal &f, qreal &t, qreal progress);
extern QVariant _q_interpolateCounterclockwiseRotation(qreal &f, qreal &t, qreal progress);

void QQuickRotationAnimatorJob::updateCurrentTime(int time)
{
#if QT_CONFIG(opengl)
    Q_ASSERT(!m_controller || !m_controller->m_window->openglContext() || m_controller->m_window->openglContext()->thread() == QThread::currentThread());
#endif
    if (!m_helper)
        return;

    float t = progress(time);

    switch (m_direction) {
    case QQuickRotationAnimator::Clockwise:
        m_value = _q_interpolateClockwiseRotation(m_from, m_to, t).toFloat();
        // The logic in _q_interpolateClockwise comes out a bit wrong
        // for the case of X->0 where 0<X<360. It ends on 360 which it
        // shouldn't.
        if (t == 1)
            m_value = m_to;
        break;
    case QQuickRotationAnimator::Counterclockwise:
        m_value = _q_interpolateCounterclockwiseRotation(m_from, m_to, t).toFloat();
        break;
    case QQuickRotationAnimator::Shortest:
        m_value = _q_interpolateShortestRotation(m_from, m_to, t).toFloat();
        break;
    case QQuickRotationAnimator::Numerical:
        m_value = m_from + (m_to - m_from) * t;
        break;
    }
    m_helper->rotation = m_value;
    m_helper->wasChanged = true;
}

void QQuickRotationAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setRotation(value());
}


QQuickOpacityAnimatorJob::QQuickOpacityAnimatorJob()
    : m_opacityNode(nullptr)
{
}

void QQuickOpacityAnimatorJob::postSync()
{
    if (!m_target) {
        invalidate();
        return;
    }

    QQuickItemPrivate *d = QQuickItemPrivate::get(m_target);
#if QT_CONFIG(quick_shadereffect)
    if (d->extra.isAllocated()
            && d->extra->layer
            && d->extra->layer->enabled()) {
        d = QQuickItemPrivate::get(d->extra->layer->m_effectSource);
    }
#endif

    m_opacityNode = d->opacityNode();

    if (!m_opacityNode) {
        m_opacityNode = new QSGOpacityNode();

        /* The item node subtree is like this
         *
         * itemNode
         * (opacityNode)            optional
         * (clipNode)               optional
         * (rootNode)               optional
         * children / paintNode
         *
         * If the opacity node doesn't exist, we need to insert it into
         * the hierarchy between itemNode and clipNode or rootNode. If
         * neither clip or root exists, we need to reparent all children
         * from itemNode to opacityNode.
         */
        QSGNode *iNode = d->itemNode();
        QSGNode *child = d->childContainerNode();
        if (child != iNode) {
            if (child->parent())
                child->parent()->removeChildNode(child);
            m_opacityNode->appendChildNode(child);
            iNode->appendChildNode(m_opacityNode);
        } else {
            iNode->reparentChildNodesTo(m_opacityNode);
            iNode->appendChildNode(m_opacityNode);
        }

        d->extra.value().opacityNode = m_opacityNode;
    }
    Q_ASSERT(m_opacityNode);
}

void QQuickOpacityAnimatorJob::invalidate()
{
    m_opacityNode = nullptr;
}

void QQuickOpacityAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setOpacity(value());
}

void QQuickOpacityAnimatorJob::updateCurrentTime(int time)
{
#if QT_CONFIG(opengl)
    Q_ASSERT(!m_controller || !m_controller->m_window->openglContext() || m_controller->m_window->openglContext()->thread() == QThread::currentThread());
#endif

    if (!m_opacityNode)
        return;

    m_value = m_from + (m_to - m_from) * progress(time);
    m_opacityNode->setOpacity(m_value);
}


#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
QQuickUniformAnimatorJob::QQuickUniformAnimatorJob()
    : m_node(nullptr)
    , m_uniformIndex(-1)
    , m_uniformType(-1)
{
    m_isUniform = true;
}

void QQuickUniformAnimatorJob::setTarget(QQuickItem *target)
{
    QQuickShaderEffect* effect = qobject_cast<QQuickShaderEffect*>(target);
    if (effect && effect->isOpenGLShaderEffect())
        m_target = target;
}

void QQuickUniformAnimatorJob::invalidate()
{
    m_node = nullptr;
    m_uniformIndex = -1;
    m_uniformType = -1;
}

void QQuickUniformAnimatorJob::postSync()
{
    if (!m_target) {
        invalidate();
        return;
    }

    m_node = static_cast<QQuickOpenGLShaderEffectNode *>(QQuickItemPrivate::get(m_target)->paintNode);

    if (m_node && m_uniformIndex == -1 && m_uniformType == -1) {
        QQuickOpenGLShaderEffectMaterial *material =
                static_cast<QQuickOpenGLShaderEffectMaterial *>(m_node->material());
        bool found = false;
        for (int t=0; !found && t<QQuickOpenGLShaderEffectMaterialKey::ShaderTypeCount; ++t) {
            const QVector<QQuickOpenGLShaderEffectMaterial::UniformData> &uniforms = material->uniforms[t];
            for (int i=0; i<uniforms.size(); ++i) {
                if (uniforms.at(i).name == m_uniform) {
                    m_uniformIndex = i;
                    m_uniformType = t;
                    found = true;
                    break;
                }
            }
        }
    }

}

void QQuickUniformAnimatorJob::updateCurrentTime(int time)
{
    if (!m_controller)
        return;

    if (!m_node || m_uniformIndex == -1 || m_uniformType == -1)
        return;

    m_value = m_from + (m_to - m_from) * progress(time);

    QQuickOpenGLShaderEffectMaterial *material =
            static_cast<QQuickOpenGLShaderEffectMaterial *>(m_node->material());
    material->uniforms[m_uniformType][m_uniformIndex].value = m_value;
    // As we're not touching the nodes, we need to explicitly mark it dirty.
    // Otherwise, the renderer will abort repainting if this was the only
    // change in the graph currently rendering.
    m_node->markDirty(QSGNode::DirtyMaterial);
}

void QQuickUniformAnimatorJob::writeBack()
{
    if (m_target)
        m_target->setProperty(m_uniform, value());
}
#endif

QT_END_NAMESPACE

#include "moc_qquickanimatorjob_p.cpp"
