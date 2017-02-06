/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qsgwindowsrenderloop_p.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QLibraryInfo>
#include <QtCore/QThread>

#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>
#include <QtGui/QOffscreenSurface>

#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qsgdefaultrendercontext_p.h>

#include <QtQuick/QQuickWindow>

#include <private/qquickprofiler_p.h>
#include <private/qquickanimatorcontroller_p.h>

#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
#include <private/qquickopenglshadereffectnode_p.h>
#endif

QT_BEGIN_NAMESPACE

extern Q_GUI_EXPORT QImage qt_gl_read_framebuffer(const QSize &size, bool alpha_format, bool include_alpha);

#define RLDEBUG(x) qCDebug(QSG_LOG_RENDERLOOP) << x;

static QElapsedTimer qsg_render_timer;
#define QSG_LOG_TIME_SAMPLE(sampleName) \
    qint64 sampleName = 0;                                                  \
    if (QSG_LOG_TIME_RENDERLOOP().isDebugEnabled())  \
        sampleName = qsg_render_timer.nsecsElapsed(); \

#define QSG_RENDER_TIMING_SAMPLE(frameType, sampleName, position) \
    QSG_LOG_TIME_SAMPLE(sampleName) \
    Q_QUICK_SG_PROFILE_RECORD(frameType, position);


QSGWindowsRenderLoop::QSGWindowsRenderLoop()
    : m_gl(0)
    , m_sg(QSGContext::createDefaultContext())
    , m_updateTimer(0)
    , m_animationTimer(0)
{
    m_rc = static_cast<QSGDefaultRenderContext *>(m_sg->createRenderContext());

    m_animationDriver = m_sg->createAnimationDriver(m_sg);
    m_animationDriver->install();

    connect(m_animationDriver, SIGNAL(started()), this, SLOT(started()));
    connect(m_animationDriver, SIGNAL(stopped()), this, SLOT(stopped()));

    m_vsyncDelta = 1000 / QGuiApplication::primaryScreen()->refreshRate();
    if (m_vsyncDelta <= 0)
        m_vsyncDelta = 16;

    RLDEBUG("Windows Render Loop created");

    qsg_render_timer.start();
}

QSGWindowsRenderLoop::~QSGWindowsRenderLoop()
{
    delete m_rc;
    delete m_sg;
}

bool QSGWindowsRenderLoop::interleaveIncubation() const
{
    return m_animationDriver->isRunning() && anyoneShowing();
}

QSGWindowsRenderLoop::WindowData *QSGWindowsRenderLoop::windowData(QQuickWindow *window)
{
    for (int i=0; i<m_windows.size(); ++i) {
        WindowData &wd = m_windows[i];
        if (wd.window == window)
            return &wd;
    }
    return 0;
}

void QSGWindowsRenderLoop::maybePostUpdateTimer()
{
    if (!m_updateTimer) {
        RLDEBUG(" - posting event");
        m_updateTimer = startTimer(m_vsyncDelta / 3);
    }
}

/*
 * If no windows are showing, start ticking animations using a timer,
 * otherwise, start rendering
 */
void QSGWindowsRenderLoop::started()
{
    RLDEBUG("Animations started...");
    if (!anyoneShowing()) {
        if (m_animationTimer == 0) {
            RLDEBUG(" - starting non-visual animation timer");
            m_animationTimer = startTimer(m_vsyncDelta);
        }
    } else {
        maybePostUpdateTimer();
    }
}

void QSGWindowsRenderLoop::stopped()
{
    RLDEBUG("Animations stopped...");
    if (m_animationTimer) {
        RLDEBUG(" - stopping non-visual animation timer");
        killTimer(m_animationTimer);
        m_animationTimer = 0;
    }
}

void QSGWindowsRenderLoop::show(QQuickWindow *window)
{
    RLDEBUG("show");
    if (windowData(window) != 0)
        return;

    // This happens before the platform window is shown, but after
    // it is created. Creating the GL context takes a lot of time
    // (hundreds of milliseconds) and will prevent us from rendering
    // the first frame in time for the initial show on screen.
    // By preparing the GL context here, it is feasible (if the app
    // is quick enough) to have a perfect first frame.
    if (!m_gl) {
        RLDEBUG(" - creating GL context");
        m_gl = new QOpenGLContext();
        m_gl->setFormat(window->requestedFormat());
        m_gl->setScreen(window->screen());
        if (qt_gl_global_share_context())
            m_gl->setShareContext(qt_gl_global_share_context());
        bool created = m_gl->create();
        if (!created) {
            const bool isEs = m_gl->isOpenGLES();
            delete m_gl;
            m_gl = 0;
            handleContextCreationFailure(window, isEs);
            return;
        }

        QQuickWindowPrivate::get(window)->fireOpenGLContextCreated(m_gl);

        RLDEBUG(" - making current");
        bool current = m_gl->makeCurrent(window);
        RLDEBUG(" - initializing SG");
        if (current)
            m_rc->initialize(m_gl);
    }

    WindowData data;
    data.window = window;
    data.pendingUpdate = false;
    m_windows << data;

    RLDEBUG(" - done with show");
}

void QSGWindowsRenderLoop::hide(QQuickWindow *window)
{
    RLDEBUG("hide");
    // The expose event is queued while hide is sent synchronously, so
    // the value might not be updated yet. (plus that the windows plugin
    // sends exposed=true when it goes to hidden, so it is doubly broken)
    // The check is made here, after the removal from m_windows, so
    // anyoneShowing will report the right value.
    if (window->isExposed())
        handleObscurity();
    if (!m_gl)
        return;
    QQuickWindowPrivate::get(window)->fireAboutToStop();
}

void QSGWindowsRenderLoop::windowDestroyed(QQuickWindow *window)
{
    RLDEBUG("windowDestroyed");
    for (int i=0; i<m_windows.size(); ++i) {
        if (m_windows.at(i).window == window) {
            m_windows.removeAt(i);
            break;
        }
    }

    hide(window);

    QQuickWindowPrivate *d = QQuickWindowPrivate::get(window);

    bool current = false;
    QScopedPointer<QOffscreenSurface> offscreenSurface;
    if (m_gl) {
        QSurface *surface = window;
        // There may be no platform window if the window got closed.
        if (!window->handle()) {
            offscreenSurface.reset(new QOffscreenSurface);
            offscreenSurface->setFormat(m_gl->format());
            offscreenSurface->create();
            surface = offscreenSurface.data();
        }
        current = m_gl->makeCurrent(surface);
    }
    if (Q_UNLIKELY(!current))
        qCDebug(QSG_LOG_RENDERLOOP) << "cleanup without an OpenGL context";

#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
    QQuickOpenGLShaderEffectMaterial::cleanupMaterialCache();
#endif

    d->cleanupNodesOnShutdown();
    if (m_windows.size() == 0) {
        d->context->invalidate();
        delete m_gl;
        m_gl = 0;
    } else if (m_gl && current) {
        m_gl->doneCurrent();
    }

    delete d->animationController;
}

bool QSGWindowsRenderLoop::anyoneShowing() const
{
    for (const WindowData &wd : qAsConst(m_windows))
        if (wd.window->isVisible() && wd.window->isExposed() && wd.window->size().isValid())
            return true;
    return false;
}

void QSGWindowsRenderLoop::exposureChanged(QQuickWindow *window)
{

    if (windowData(window) == 0)
        return;

    if (window->isExposed() && window->isVisible()) {

        // Stop non-visual animation timer as we now have a window rendering
        if (m_animationTimer && anyoneShowing()) {
            RLDEBUG(" - stopping non-visual animation timer");
            killTimer(m_animationTimer);
            m_animationTimer = 0;
        }

        RLDEBUG("exposureChanged - exposed");
        WindowData *wd = windowData(window);
        wd->pendingUpdate = true;

        // If we have a pending timer and we get an expose, we need to stop it.
        // Otherwise we get two frames and two animation ticks in the same time-interval.
        if (m_updateTimer) {
            RLDEBUG(" - killing pending update timer");
            killTimer(m_updateTimer);
            m_updateTimer = 0;
        }
        render();
    } else {
        handleObscurity();
    }
}

void QSGWindowsRenderLoop::handleObscurity()
{
    RLDEBUG("handleObscurity");
    // Potentially start the non-visual animation timer if nobody is rendering
    if (m_animationDriver->isRunning() && !anyoneShowing() && !m_animationTimer) {
        RLDEBUG(" - starting non-visual animation timer");
        m_animationTimer = startTimer(m_vsyncDelta);
    }
}

QImage QSGWindowsRenderLoop::grab(QQuickWindow *window)
{
    RLDEBUG("grab");
    if (!m_gl)
        return QImage();

    m_gl->makeCurrent(window);

    QQuickWindowPrivate *d = QQuickWindowPrivate::get(window);
    d->polishItems();
    d->syncSceneGraph();
    d->renderSceneGraph(window->size());

    bool alpha = window->format().alphaBufferSize() > 0 && window->color().alpha() != 255;
    QImage image = qt_gl_read_framebuffer(window->size() * window->effectiveDevicePixelRatio(), alpha, alpha);
    return image;
}

void QSGWindowsRenderLoop::update(QQuickWindow *window)
{
    RLDEBUG("update");
    maybeUpdate(window);
}

void QSGWindowsRenderLoop::maybeUpdate(QQuickWindow *window)
{
    RLDEBUG("maybeUpdate");

    WindowData *wd = windowData(window);
    if (!wd || !anyoneShowing())
        return;

    wd->pendingUpdate = true;
    maybePostUpdateTimer();
}

QSGRenderContext *QSGWindowsRenderLoop::createRenderContext(QSGContext *) const
{
    return m_rc;
}

bool QSGWindowsRenderLoop::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Timer: {
        QTimerEvent *te = static_cast<QTimerEvent *>(event);
        if (te->timerId() == m_animationTimer) {
            RLDEBUG("event : animation tick while nothing is showing");
            m_animationDriver->advance();
        } else if (te->timerId() == m_updateTimer) {
            RLDEBUG("event : update");
            killTimer(m_updateTimer);
            m_updateTimer = 0;
            render();
        }
        return true; }
    default:
        break;
    }

    return QObject::event(event);
}

/*
 * Go through all windows we control and render them in turn.
 * Then tick animations if active.
 */
void QSGWindowsRenderLoop::render()
{
    RLDEBUG("render");
    bool rendered = false;
    for (const WindowData &wd : qAsConst(m_windows)) {
        if (wd.pendingUpdate) {
            const_cast<WindowData &>(wd).pendingUpdate = false;
            renderWindow(wd.window);
            rendered = true;
        }
    }

    if (!rendered) {
        RLDEBUG("no changes, sleep");
        QThread::msleep(m_vsyncDelta);
    }

    if (m_animationDriver->isRunning()) {
        RLDEBUG("advancing animations");
        QSG_LOG_TIME_SAMPLE(time_start);
        Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphWindowsAnimations);
        m_animationDriver->advance();
        RLDEBUG("animations advanced");

        qCDebug(QSG_LOG_TIME_RENDERLOOP,
                "animations ticked in %dms",
                int((qsg_render_timer.nsecsElapsed() - time_start)/1000000));

        Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphWindowsAnimations, 1);

        // It is not given that animations triggered another maybeUpdate()
        // and thus another render pass, so to keep things running,
        // make sure there is another frame pending.
        maybePostUpdateTimer();

        emit timeToIncubate();
    }
}

/*
 * Render the contents of this window. First polish, then sync, render
 * then finally swap.
 *
 * Note: This render function does not implement aborting
 * the render call when sync step results in no scene graph changes,
 * like the threaded renderer does.
 */
void QSGWindowsRenderLoop::renderWindow(QQuickWindow *window)
{
    RLDEBUG("renderWindow");
    QQuickWindowPrivate *d = QQuickWindowPrivate::get(window);

    if (!d->isRenderable())
        return;

    if (!m_gl->makeCurrent(window)) {
        // Check for context loss.
        if (!m_gl->isValid()) {
            d->cleanupNodesOnShutdown();
            m_rc->invalidate();
            if (m_gl->create() && m_gl->makeCurrent(window))
                m_rc->initialize(m_gl);
            else
                return;
        }
    }

    d->flushFrameSynchronousEvents();
    // Event delivery or processing has caused the window to stop rendering.
    if (!windowData(window))
        return;

    QSG_LOG_TIME_SAMPLE(time_start);
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphPolishFrame);

    RLDEBUG(" - polishing");
    d->polishItems();
    QSG_LOG_TIME_SAMPLE(time_polished);
    Q_QUICK_SG_PROFILE_SWITCH(QQuickProfiler::SceneGraphPolishFrame,
                              QQuickProfiler::SceneGraphRenderLoopFrame,
                              QQuickProfiler::SceneGraphPolishPolish);

    emit window->afterAnimating();

    RLDEBUG(" - syncing");
    d->syncSceneGraph();
    QSG_RENDER_TIMING_SAMPLE(QQuickProfiler::SceneGraphRenderLoopFrame, time_synced,
                             QQuickProfiler::SceneGraphRenderLoopSync);

    RLDEBUG(" - rendering");
    d->renderSceneGraph(window->size());
    QSG_RENDER_TIMING_SAMPLE(QQuickProfiler::SceneGraphRenderLoopFrame, time_rendered,
                             QQuickProfiler::SceneGraphRenderLoopRender);

    RLDEBUG(" - swapping");
    if (!d->customRenderStage || !d->customRenderStage->swap())
        m_gl->swapBuffers(window);
    QSG_RENDER_TIMING_SAMPLE(QQuickProfiler::SceneGraphRenderLoopFrame, time_swapped,
                             QQuickProfiler::SceneGraphRenderLoopSwap);

    RLDEBUG(" - frameDone");
    d->fireFrameSwapped();

    qCDebug(QSG_LOG_TIME_RENDERLOOP()).nospace()
            << "Frame rendered with 'windows' renderloop in: " << (time_swapped - time_start) / 1000000 << "ms"
            << ", polish=" << (time_polished - time_start) / 1000000
            << ", sync=" << (time_synced - time_polished) / 1000000
            << ", render=" << (time_rendered - time_synced) / 1000000
            << ", swap=" << (time_swapped - time_rendered) / 1000000
            << " - " << window;

    Q_QUICK_SG_PROFILE_REPORT(QQuickProfiler::SceneGraphRenderLoopFrame,
                              QQuickProfiler::SceneGraphRenderLoopSwap);
}

QT_END_NAMESPACE
