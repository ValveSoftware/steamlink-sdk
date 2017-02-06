/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "scene3drenderer_p.h"
#include "scene3dcleaner_p.h"
#include "scene3ditem_p.h"
#include "scene3dlogging_p.h"
#include "scene3dsgnode_p.h"

#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DCore/qaspectengine.h>
#include <Qt3DCore/private/qaspectengine_p.h>

#include <QtQuick/qquickwindow.h>

#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglframebufferobject.h>

#include <QtCore/qthread.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class ContextSaver
{
public:
    explicit ContextSaver(QOpenGLContext *context = QOpenGLContext::currentContext())
        : m_context(context),
          m_surface(context ? context->surface() : nullptr)
    {
    }

    ~ContextSaver()
    {
        if (m_context && m_context->surface() != m_surface)
            m_context->makeCurrent(m_surface);
    }

    QOpenGLContext *context() const { return m_context; }
    QSurface *surface() const { return m_surface; }

private:
    QOpenGLContext * const m_context;
    QSurface * const m_surface;
};

/*!
    \class Qt3DCore::Scene3DRenderer
    \internal

    \brief The Qt3DCore::Scene3DRenderer class takes care of rendering a Qt3D scene
    within a Framebuffer object to be used by the QtQuick 2 renderer.

    The Qt3DCore::Scene3DRenderer class renders a Qt3D scene as provided by a Qt3DCore::Scene3DItem.
    It owns the aspectEngine even though it doesn't instantiate it.

    The shutdown procedure is a two steps process that goes as follow:

    \li The window is closed

    \li This triggers the windowsChanged signal which the Scene3DRenderer
    uses to perform the necessary cleanups in the QSGRenderThread (destroys
    DebugLogger ...) with the shutdown slot (queued connection).

    \li The destroyed signal of the window is also connected to the
    Scene3DRenderer. When triggered in the context of the main thread, the
    cleanup slot is called.

    There is an alternate shutdown procedure in case the QQuickItem is
    destroyed with an active window which can happen in the case where the
    Scene3D is used with a QtQuick Loader

    In that case the shutdown procedure goes the same except that the destroyed
    signal of the window is not called. Therefore the cleanup method is invoked
    to properly destroy the aspect engine.
 */
Scene3DRenderer::Scene3DRenderer(Scene3DItem *item, Qt3DCore::QAspectEngine *aspectEngine, QRenderAspect *renderAspect)
    : QObject()
    , m_item(item)
    , m_aspectEngine(aspectEngine)
    , m_renderAspect(renderAspect)
    , m_multisampledFBO(nullptr)
    , m_finalFBO(nullptr)
    , m_texture(nullptr)
    , m_multisample(false) // this value is not used, will be synced from the Scene3DItem instead
    , m_lastMultisample(false)
{
    Q_CHECK_PTR(m_item);
    Q_CHECK_PTR(m_item->window());

    QObject::connect(m_item->window(), &QQuickWindow::beforeRendering, this, &Scene3DRenderer::render, Qt::DirectConnection);
    QObject::connect(m_item, &QQuickItem::windowChanged, this, &Scene3DRenderer::onWindowChangedQueued, Qt::QueuedConnection);

    Q_ASSERT(QOpenGLContext::currentContext());
    ContextSaver saver;
    static_cast<QRenderAspectPrivate*>(QRenderAspectPrivate::get(m_renderAspect))->renderInitialize(saver.context());
    scheduleRootEntityChange();
}

Scene3DRenderer::~Scene3DRenderer()
{
    qCDebug(Scene3D) << Q_FUNC_INFO << QThread::currentThread();
}

QOpenGLFramebufferObject *Scene3DRenderer::createMultisampledFramebufferObject(const QSize &size)
{
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    int samples = QSurfaceFormat::defaultFormat().samples();
    if (samples == -1)
        samples = 4;
    format.setSamples(samples);
    return new QOpenGLFramebufferObject(size, format);
}

QOpenGLFramebufferObject *Scene3DRenderer::createFramebufferObject(const QSize &size)
{
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    return new QOpenGLFramebufferObject(size, format);
}

void Scene3DRenderer::scheduleRootEntityChange()
{
    QMetaObject::invokeMethod(m_item, "applyRootEntityChange", Qt::QueuedConnection);
}

void Scene3DRenderer::setCleanerHelper(Scene3DCleaner *cleaner)
{
    m_cleaner = cleaner;
    if (m_cleaner) {
        // Window closed case
        QObject::connect(m_item->window(), &QQuickWindow::destroyed, m_cleaner, &Scene3DCleaner::cleanup);
        m_cleaner->setRenderer(this);
    }
}

// Executed in the QtQuick render thread.
void Scene3DRenderer::shutdown()
{
    qCDebug(Scene3D) << Q_FUNC_INFO << QThread::currentThread();

    // Set to null so that subsequent calls to render
    // would return early
    m_item = nullptr;

    // Exit the simulation loop so no more jobs are asked for. Once this
    // returns it is safe to shutdown the renderer.
    if (m_aspectEngine) {
        auto engineD = Qt3DCore::QAspectEnginePrivate::get(m_aspectEngine);
        engineD->exitSimulationLoop();
    }

    // Shutdown the Renderer Aspect while the OpenGL context
    // is still valid
    if (m_renderAspect)
        static_cast<QRenderAspectPrivate*>(QRenderAspectPrivate::get(m_renderAspect))->renderShutdown();
}

// SGThread
void Scene3DRenderer::onWindowChangedQueued(QQuickWindow *w)
{
    if (w == nullptr) {
        qCDebug(Scene3D) << Q_FUNC_INFO << QThread::currentThread();
        shutdown();
        // Will only trigger something with the Loader case
        // The window closed cases is handled by the window's destroyed
        // signal
        QMetaObject::invokeMethod(m_cleaner, "cleanup");
    }
}

void Scene3DRenderer::synchronize()
{
    m_multisample = m_item->multisample();
}

void Scene3DRenderer::setSGNode(Scene3DSGNode *node)
{
    m_node = node;
    if (!m_texture.isNull())
        node->setTexture(m_texture.data());
}

void Scene3DRenderer::render()
{
    if (!m_item || !m_item->window())
        return;

    QQuickWindow *window = m_item->window();

    if (m_aspectEngine->rootEntity() != m_item->entity())
        scheduleRootEntityChange();

    ContextSaver saver;

    const QSize boundingRectSize = m_item->boundingRect().size().toSize();
    const QSize currentSize = boundingRectSize * window->effectiveDevicePixelRatio();
    const bool sizeHasChanged = currentSize != m_lastSize;
    const bool multisampleHasChanged = m_multisample != m_lastMultisample;
    const bool forceRecreate = sizeHasChanged || multisampleHasChanged;

    if (sizeHasChanged)
        m_item->setItemArea(boundingRectSize);

    // Rebuild FBO and textures if never created or a resize has occurred
    if ((m_multisampledFBO.isNull() || forceRecreate) && m_multisample) {
        m_multisampledFBO.reset(createMultisampledFramebufferObject(currentSize));
        if (m_multisampledFBO->format().samples() == 0 || !QOpenGLFramebufferObject::hasOpenGLFramebufferBlit()) {
            m_multisample = false;
            m_multisampledFBO.reset(nullptr);
        }
    }

    if (m_finalFBO.isNull() || forceRecreate) {
        m_finalFBO.reset(createFramebufferObject(currentSize));
        m_texture.reset(window->createTextureFromId(m_finalFBO->texture(), m_finalFBO->size(), QQuickWindow::TextureHasAlphaChannel));
        m_node->setTexture(m_texture.data());
    }

    // Store the current size as a comparison
    // point for the next frame
    m_lastSize = currentSize;
    m_lastMultisample = m_multisample;

    // Bind FBO
    if (m_multisample) //Only try to use MSAA when available
        m_multisampledFBO->bind();
    else
        m_finalFBO->bind();

    // Render Qt3D Scene
    static_cast<QRenderAspectPrivate*>(QRenderAspectPrivate::get(m_renderAspect))->renderSynchronous();

    // We may have called doneCurrent() so restore the context if the rendering surface was changed
    // Note: keep in mind that the ContextSave also restores the surface when destroyed
    if (saver.context()->surface() != saver.surface())
        saver.context()->makeCurrent(saver.surface());

    if (m_multisample) {
        // Blit multisampled FBO with non multisampled FBO with texture attachment
        const QRect dstRect(QPoint(0, 0), m_finalFBO->size());
        const QRect srcRect(QPoint(0, 0), m_multisampledFBO->size());
        QOpenGLFramebufferObject::blitFramebuffer(m_finalFBO.data(), dstRect,
                                                  m_multisampledFBO.data(), srcRect,
                                                  GL_COLOR_BUFFER_BIT,
                                                  GL_NEAREST,
                                                  0, 0,
                                                  QOpenGLFramebufferObject::DontRestoreFramebufferBinding);
    }

    // Restore QtQuick FBO
    QOpenGLFramebufferObject::bindDefault();

    // Reset the state used by the Qt Quick scenegraph to avoid any
    // interference when rendering the rest of the UI.
    window->resetOpenGLState();

    // Mark material as dirty to request a new frame
    m_node->markDirty(QSGNode::DirtyMaterial);

    // Request next frame
    window->update();
}

} // namespace Qt3DRender

QT_END_NAMESPACE
