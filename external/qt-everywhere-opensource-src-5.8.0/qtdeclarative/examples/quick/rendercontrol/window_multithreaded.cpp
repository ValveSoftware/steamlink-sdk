/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "window_multithreaded.h"
#include "cuberenderer.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOffscreenSurface>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickRenderControl>
#include <QCoreApplication>

/*
  This implementation runs the Qt Quick scenegraph's sync and render phases on a
  separate, dedicated thread.  Rendering the cube using our custom OpenGL engine
  happens on that thread as well.  This is similar to the built-in threaded
  render loop, but does not support all the features. There is no support for
  getting Animators running on the render thread for example.

  We choose to use QObject's event mechanism to communicate with the QObject
  living on the render thread. An alternative would be to subclass QThread and
  reimplement run() with a custom event handling approach, like
  QSGThreadedRenderLoop does. That would potentially lead to better results but
  is also more complex.
*/

static const QEvent::Type INIT = QEvent::Type(QEvent::User + 1);
static const QEvent::Type RENDER = QEvent::Type(QEvent::User + 2);
static const QEvent::Type RESIZE = QEvent::Type(QEvent::User + 3);
static const QEvent::Type STOP = QEvent::Type(QEvent::User + 4);

static const QEvent::Type UPDATE = QEvent::Type(QEvent::User + 5);

QuickRenderer::QuickRenderer()
    : m_context(0),
      m_surface(0),
      m_fbo(0),
      m_window(0),
      m_quickWindow(0),
      m_renderControl(0),
      m_cubeRenderer(0),
      m_quit(false)
{
}

void QuickRenderer::requestInit()
{
    QCoreApplication::postEvent(this, new QEvent(INIT));
}

void QuickRenderer::requestRender()
{
    QCoreApplication::postEvent(this, new QEvent(RENDER));
}

void QuickRenderer::requestResize()
{
    QCoreApplication::postEvent(this, new QEvent(RESIZE));
}

void QuickRenderer::requestStop()
{
    QCoreApplication::postEvent(this, new QEvent(STOP));
}

bool QuickRenderer::event(QEvent *e)
{
    QMutexLocker lock(&m_mutex);

    switch (int(e->type())) {
    case INIT:
        init();
        return true;
    case RENDER:
        render(&lock);
        return true;
    case RESIZE:
        if (m_cubeRenderer)
            m_cubeRenderer->resize(m_window->width(), m_window->height());
        return true;
    case STOP:
        cleanup();
        return true;
    default:
        return QObject::event(e);
    }
}

void QuickRenderer::init()
{
    m_context->makeCurrent(m_surface);

    // Pass our offscreen surface to the cube renderer just so that it will
    // have something is can make current during cleanup. QOffscreenSurface,
    // just like QWindow, must always be created on the gui thread (as it might
    // be backed by an actual QWindow).
    m_cubeRenderer = new CubeRenderer(m_surface);
    m_cubeRenderer->resize(m_window->width(), m_window->height());

    m_renderControl->initialize(m_context);
}

void QuickRenderer::cleanup()
{
    m_context->makeCurrent(m_surface);

    m_renderControl->invalidate();

    delete m_fbo;
    m_fbo = 0;

    delete m_cubeRenderer;
    m_cubeRenderer = 0;

    m_context->doneCurrent();
    m_context->moveToThread(QCoreApplication::instance()->thread());

    m_cond.wakeOne();
}

void QuickRenderer::ensureFbo()
{
    if (m_fbo && m_fbo->size() != m_window->size() * m_window->devicePixelRatio()) {
        delete m_fbo;
        m_fbo = 0;
    }

    if (!m_fbo) {
        m_fbo = new QOpenGLFramebufferObject(m_window->size() * m_window->devicePixelRatio(),
                                             QOpenGLFramebufferObject::CombinedDepthStencil);
        m_quickWindow->setRenderTarget(m_fbo);
    }
}

void QuickRenderer::render(QMutexLocker *lock)
{
    Q_ASSERT(QThread::currentThread() != m_window->thread());

    if (!m_context->makeCurrent(m_surface)) {
        qWarning("Failed to make context current on render thread");
        return;
    }

    ensureFbo();

    // Synchronization and rendering happens here on the render thread.
    m_renderControl->sync();

    // The gui thread can now continue.
    m_cond.wakeOne();
    lock->unlock();

    // Meanwhile on this thread continue with the actual rendering (into the FBO first).
    m_renderControl->render();
    m_context->functions()->glFlush();

    // The cube renderer uses its own context, no need to bother with the state here.

    // Get something onto the screen using our custom OpenGL engine.
    QMutexLocker quitLock(&m_quitMutex);
    if (!m_quit)
        m_cubeRenderer->render(m_window, m_context, m_fbo->texture());
}

void QuickRenderer::aboutToQuit()
{
    QMutexLocker lock(&m_quitMutex);
    m_quit = true;
}

class RenderControl : public QQuickRenderControl
{
public:
    RenderControl(QWindow *w) : m_window(w) { }
    QWindow *renderWindow(QPoint *offset) Q_DECL_OVERRIDE;

private:
    QWindow *m_window;
};

WindowMultiThreaded::WindowMultiThreaded()
    : m_qmlComponent(Q_NULLPTR),
      m_rootItem(0),
      m_quickInitialized(false),
      m_psrRequested(false)
{
    setSurfaceType(QSurface::OpenGLSurface);

    QSurfaceFormat format;
    // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(8);
    setFormat(format);

    m_context = new QOpenGLContext;
    m_context->setFormat(format);
    m_context->create();

    m_offscreenSurface = new QOffscreenSurface;
    // Pass m_context->format(), not format. Format does not specify and color buffer
    // sizes, while the context, that has just been created, reports a format that has
    // these values filled in. Pass this to the offscreen surface to make sure it will be
    // compatible with the context's configuration.
    m_offscreenSurface->setFormat(m_context->format());
    m_offscreenSurface->create();

    m_renderControl = new RenderControl(this);

    // Create a QQuickWindow that is associated with out render control. Note that this
    // window never gets created or shown, meaning that it will never get an underlying
    // native (platform) window.
    m_quickWindow = new QQuickWindow(m_renderControl);

    // Create a QML engine.
    m_qmlEngine = new QQmlEngine;
    if (!m_qmlEngine->incubationController())
        m_qmlEngine->setIncubationController(m_quickWindow->incubationController());

    m_quickRenderer = new QuickRenderer;
    m_quickRenderer->setContext(m_context);

    // These live on the gui thread. Just give access to them on the render thread.
    m_quickRenderer->setSurface(m_offscreenSurface);
    m_quickRenderer->setWindow(this);
    m_quickRenderer->setQuickWindow(m_quickWindow);
    m_quickRenderer->setRenderControl(m_renderControl);

    m_quickRendererThread = new QThread;

    // Notify the render control that some scenegraph internals have to live on
    // m_quickRenderThread.
    m_renderControl->prepareThread(m_quickRendererThread);

    // The QOpenGLContext and the QObject representing the rendering logic on
    // the render thread must live on that thread.
    m_context->moveToThread(m_quickRendererThread);
    m_quickRenderer->moveToThread(m_quickRendererThread);

    m_quickRendererThread->start();

    // Now hook up the signals. For simplicy we don't differentiate
    // between renderRequested (only render is needed, no sync) and
    // sceneChanged (polish and sync is needed too).
    connect(m_renderControl, &QQuickRenderControl::renderRequested, this, &WindowMultiThreaded::requestUpdate);
    connect(m_renderControl, &QQuickRenderControl::sceneChanged, this, &WindowMultiThreaded::requestUpdate);
}

WindowMultiThreaded::~WindowMultiThreaded()
{
    // Release resources and move the context ownership back to this thread.
    m_quickRenderer->mutex()->lock();
    m_quickRenderer->requestStop();
    m_quickRenderer->cond()->wait(m_quickRenderer->mutex());
    m_quickRenderer->mutex()->unlock();

    m_quickRendererThread->quit();
    m_quickRendererThread->wait();

    delete m_renderControl;
    delete m_qmlComponent;
    delete m_quickWindow;
    delete m_qmlEngine;

    delete m_offscreenSurface;
    delete m_context;
}

void WindowMultiThreaded::requestUpdate()
{
    if (m_quickInitialized && !m_psrRequested) {
        m_psrRequested = true;
        QCoreApplication::postEvent(this, new QEvent(UPDATE));
    }
}

bool WindowMultiThreaded::event(QEvent *e)
{
    if (e->type() == UPDATE) {
        polishSyncAndRender();
        m_psrRequested = false;
        return true;
    } else if (e->type() == QEvent::Close) {
        // Avoid rendering on the render thread when the window is about to
        // close. Once a QWindow is closed, the underlying platform window will
        // go away, even though the QWindow instance itself is still
        // valid. Operations like swapBuffers() are futile and only result in
        // warnings afterwards. Prevent this.
        m_quickRenderer->aboutToQuit();
    }
    return QWindow::event(e);
}

void WindowMultiThreaded::polishSyncAndRender()
{
    Q_ASSERT(QThread::currentThread() == thread());

    // Polishing happens on the gui thread.
    m_renderControl->polishItems();
    // Sync happens on the render thread with the gui thread (this one) blocked.
    QMutexLocker lock(m_quickRenderer->mutex());
    m_quickRenderer->requestRender();
    // Wait until sync is complete.
    m_quickRenderer->cond()->wait(m_quickRenderer->mutex());
    // Rendering happens on the render thread without blocking the gui (main)
    // thread. This is good because the blocking swap (waiting for vsync)
    // happens on the render thread, not blocking other work.
}

void WindowMultiThreaded::run()
{
    disconnect(m_qmlComponent, &QQmlComponent::statusChanged, this, &WindowMultiThreaded::run);

    if (m_qmlComponent->isError()) {
        QList<QQmlError> errorList = m_qmlComponent->errors();
        foreach (const QQmlError &error, errorList)
            qWarning() << error.url() << error.line() << error;
        return;
    }

    QObject *rootObject = m_qmlComponent->create();
    if (m_qmlComponent->isError()) {
        QList<QQmlError> errorList = m_qmlComponent->errors();
        foreach (const QQmlError &error, errorList)
            qWarning() << error.url() << error.line() << error;
        return;
    }

    m_rootItem = qobject_cast<QQuickItem *>(rootObject);
    if (!m_rootItem) {
        qWarning("run: Not a QQuickItem");
        delete rootObject;
        return;
    }

    // The root item is ready. Associate it with the window.
    m_rootItem->setParentItem(m_quickWindow->contentItem());

    // Update item and rendering related geometries.
    updateSizes();

    m_quickInitialized = true;

    // Initialize the render thread and perform the first polish/sync/render.
    m_quickRenderer->requestInit();
    polishSyncAndRender();
}

void WindowMultiThreaded::updateSizes()
{
    // Behave like SizeRootObjectToView.
    m_rootItem->setWidth(width());
    m_rootItem->setHeight(height());

    m_quickWindow->setGeometry(0, 0, width(), height());
}

void WindowMultiThreaded::startQuick(const QString &filename)
{
    m_qmlComponent = new QQmlComponent(m_qmlEngine, QUrl(filename));
    if (m_qmlComponent->isLoading())
        connect(m_qmlComponent, &QQmlComponent::statusChanged, this, &WindowMultiThreaded::run);
    else
        run();
}

void WindowMultiThreaded::exposeEvent(QExposeEvent *)
{
    if (isExposed()) {
        if (!m_quickInitialized)
            startQuick(QStringLiteral("qrc:/rendercontrol/demo.qml"));
    }
}

void WindowMultiThreaded::resizeEvent(QResizeEvent *)
{
    // If this is a resize after the scene is up and running, recreate the fbo and the
    // Quick item and scene.
    if (m_rootItem) {
        updateSizes();
        m_quickRenderer->requestResize();
        polishSyncAndRender();
    }
}

void WindowMultiThreaded::mousePressEvent(QMouseEvent *e)
{
    // Use the constructor taking localPos and screenPos. That puts localPos into the
    // event's localPos and windowPos, and screenPos into the event's screenPos. This way
    // the windowPos in e is ignored and is replaced by localPos. This is necessary
    // because QQuickWindow thinks of itself as a top-level window always.
    QMouseEvent mappedEvent(e->type(), e->localPos(), e->screenPos(), e->button(), e->buttons(), e->modifiers());
    QCoreApplication::sendEvent(m_quickWindow, &mappedEvent);
}

void WindowMultiThreaded::mouseReleaseEvent(QMouseEvent *e)
{
    QMouseEvent mappedEvent(e->type(), e->localPos(), e->screenPos(), e->button(), e->buttons(), e->modifiers());
    QCoreApplication::sendEvent(m_quickWindow, &mappedEvent);
}
