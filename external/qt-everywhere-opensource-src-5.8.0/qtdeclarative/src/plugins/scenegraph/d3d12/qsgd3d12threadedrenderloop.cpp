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

#include "qsgd3d12threadedrenderloop_p.h"
#include "qsgd3d12engine_p.h"
#include "qsgd3d12context_p.h"
#include "qsgd3d12rendercontext_p.h"
#include "qsgd3d12shadereffectnode_p.h"
#include <private/qsgrenderer_p.h>
#include <private/qquickwindow_p.h>
#include <private/qquickanimatorcontroller_p.h>
#include <private/qquickprofiler_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qqmldebugconnector_p.h>
#include <QElapsedTimer>
#include <QQueue>
#include <QGuiApplication>

QT_BEGIN_NAMESPACE

// NOTE: Avoid categorized logging. It is slow.

#define DECLARE_DEBUG_VAR(variable) \
    static bool debug_ ## variable() \
    { static bool value = qgetenv("QSG_RENDERER_DEBUG").contains(QT_STRINGIFY(variable)); return value; }

DECLARE_DEBUG_VAR(loop)
DECLARE_DEBUG_VAR(time)


// NOTE: The threaded renderloop is not currently safe to use in practice as it
// is prone to deadlocks, in particular when multiple windows are active. This
// is because DXGI's limitation of relying on the gui message pump in certain
// cases. See
// https://msdn.microsoft.com/en-us/library/windows/desktop/ee417025(v=vs.85).aspx#multithreading_and_dxgi
//
// This means that if swap chain functions like create, release, and
// potentially even Present, are called outside the gui thread, then the
// application must ensure the gui thread does not ever block and wait for the
// render thread - since on the render thread a DXGI call may be in turn
// waiting for the gui thread to deliver a window message...
//
// Ensuring this is impossible with the current design where the gui thread
// must block at certain points, waiting for the render thread. Qt moves out
// rendering from the main thread, in order to make application's life easier,
// whereas the typical DXGI-compatible model would require moving work, but not
// windowing and presenting, out to additional threads.


/*
   The D3D render loop mostly mirrors the threaded OpenGL render loop.

   There are two classes here. QSGD3D12ThreadedRenderLoop and
   QSGD3D12RenderThread. All communication between the two is based on event
   passing and we have a number of custom events.

   Render loop is per process, render thread is per window. The
   QSGD3D12RenderContext and QSGD3D12Engine are per window as well. The former
   is created (but not owned) by QQuickWindow. The D3D device is per process.

   In this implementation, the render thread is never blocked and the GUI
   thread will initiate a polishAndSync which will block and wait for the
   render thread to pick it up and release the block only after the render
   thread is done syncing. The reason for this is:

   1. Clear blocking paradigm. We only have one real "block" point
   (polishAndSync()) and all blocking is initiated by GUI and picked up by
   Render at specific times based on events. This makes the execution
   deterministic.

   2. Render does not have to interact with GUI. This is done so that the
   render thread can run its own animation system which stays alive even when
   the GUI thread is blocked doing I/O, object instantiation, QPainter-painting
   or any other non-trivial task.

   The render thread has affinity to the GUI thread until a window is shown.
   From that moment and until the window is destroyed, it will have affinity to
   the render thread. (moved back at the end of run for cleanup).
 */

// Passed from the RL to the RT when a window is removed obscured and should be
// removed from the render loop.
const QEvent::Type WM_Obscure           = QEvent::Type(QEvent::User + 1);

// Passed from the RL to RT when GUI has been locked, waiting for sync.
const QEvent::Type WM_RequestSync       = QEvent::Type(QEvent::User + 2);

// Passed by the RT to itself to trigger another render pass. This is typically
// a result of QQuickWindow::update().
const QEvent::Type WM_RequestRepaint    = QEvent::Type(QEvent::User + 3);

// Passed by the RL to the RT to maybe release resource if no windows are
// rendering.
const QEvent::Type WM_TryRelease        = QEvent::Type(QEvent::User + 4);

// Passed by the RL to the RT when a QQuickWindow::grabWindow() is called.
const QEvent::Type WM_Grab              = QEvent::Type(QEvent::User + 5);

// Passed by the window when there is a render job to run.
const QEvent::Type WM_PostJob           = QEvent::Type(QEvent::User + 6);

class QSGD3D12WindowEvent : public QEvent
{
public:
    QSGD3D12WindowEvent(QQuickWindow *c, QEvent::Type type) : QEvent(type), window(c) { }
    QQuickWindow *window;
};

class QSGD3D12TryReleaseEvent : public QSGD3D12WindowEvent
{
public:
    QSGD3D12TryReleaseEvent(QQuickWindow *win, bool destroy)
        : QSGD3D12WindowEvent(win, WM_TryRelease), destroying(destroy) { }
    bool destroying;
};

class QSGD3D12SyncEvent : public QSGD3D12WindowEvent
{
public:
    QSGD3D12SyncEvent(QQuickWindow *c, bool inExpose, bool force)
        : QSGD3D12WindowEvent(c, WM_RequestSync)
        , size(c->size())
        , dpr(c->effectiveDevicePixelRatio())
        , syncInExpose(inExpose)
        , forceRenderPass(force) { }
    QSize size;
    float dpr;
    bool syncInExpose;
    bool forceRenderPass;
};

class QSGD3D12GrabEvent : public QSGD3D12WindowEvent
{
public:
    QSGD3D12GrabEvent(QQuickWindow *c, QImage *result)
        : QSGD3D12WindowEvent(c, WM_Grab), image(result) { }
    QImage *image;
};

class QSGD3D12JobEvent : public QSGD3D12WindowEvent
{
public:
    QSGD3D12JobEvent(QQuickWindow *c, QRunnable *postedJob)
        : QSGD3D12WindowEvent(c, WM_PostJob), job(postedJob) { }
    ~QSGD3D12JobEvent() { delete job; }
    QRunnable *job;
};

class QSGD3D12EventQueue : public QQueue<QEvent *>
{
public:
    void addEvent(QEvent *e) {
        mutex.lock();
        enqueue(e);
        if (waiting)
            condition.wakeOne();
        mutex.unlock();
    }

    QEvent *takeEvent(bool wait) {
        mutex.lock();
        if (isEmpty() && wait) {
            waiting = true;
            condition.wait(&mutex);
            waiting = false;
        }
        QEvent *e = dequeue();
        mutex.unlock();
        return e;
    }

    bool hasMoreEvents() {
        mutex.lock();
        bool has = !isEmpty();
        mutex.unlock();
        return has;
    }

private:
    QMutex mutex;
    QWaitCondition condition;
    bool waiting = false;
};

static inline int qsgrl_animation_interval()
{
    const qreal refreshRate = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->refreshRate() : 0;
    return refreshRate < 1 ? 16 : int(1000 / refreshRate);
}

class QSGD3D12RenderThread : public QThread
{
    Q_OBJECT

public:
    QSGD3D12RenderThread(QSGD3D12ThreadedRenderLoop *rl, QSGRenderContext *renderContext)
        : renderLoop(rl)
    {
        rc = static_cast<QSGD3D12RenderContext *>(renderContext);
        vsyncDelta = qsgrl_animation_interval();
    }

    ~QSGD3D12RenderThread()
    {
        delete rc;
    }

    bool event(QEvent *e);
    void run();

    void syncAndRender();
    void sync(bool inExpose);

    void requestRepaint()
    {
        if (sleeping)
            stopEventProcessing = true;
        if (exposedWindow)
            pendingUpdate |= RepaintRequest;
    }

    void processEventsAndWaitForMore();
    void processEvents();
    void postEvent(QEvent *e);

    enum UpdateRequest {
        SyncRequest         = 0x01,
        RepaintRequest      = 0x02,
        ExposeRequest       = 0x04 | RepaintRequest | SyncRequest
    };

    QSGD3D12Engine *engine = nullptr;
    QSGD3D12ThreadedRenderLoop *renderLoop;
    QSGD3D12RenderContext *rc;
    QAnimationDriver *rtAnim = nullptr;
    volatile bool active = false;
    uint pendingUpdate = 0;
    bool sleeping = false;
    bool syncResultedInChanges = false;
    float vsyncDelta;
    QMutex mutex;
    QWaitCondition waitCondition;
    QQuickWindow *exposedWindow = nullptr;
    bool stopEventProcessing = false;
    QSGD3D12EventQueue eventQueue;
    QElapsedTimer threadTimer;
    qint64 syncTime;
    qint64 renderTime;
    qint64 sinceLastTime;

public slots:
    void onSceneGraphChanged() {
        syncResultedInChanges = true;
    }
};

bool QSGD3D12RenderThread::event(QEvent *e)
{
    switch (e->type()) {

    case WM_Obscure:
        Q_ASSERT(!exposedWindow || exposedWindow == static_cast<QSGD3D12WindowEvent *>(e)->window);
        if (Q_UNLIKELY(debug_loop()))
            qDebug() << "RT - WM_Obscure" << exposedWindow;
        mutex.lock();
        if (exposedWindow) {
            QQuickWindowPrivate::get(exposedWindow)->fireAboutToStop();
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - WM_Obscure - window removed");
            exposedWindow = nullptr;
        }
        waitCondition.wakeOne();
        mutex.unlock();
        return true;

    case WM_RequestSync: {
        QSGD3D12SyncEvent *wme = static_cast<QSGD3D12SyncEvent *>(e);
        if (sleeping)
            stopEventProcessing = true;
        // One thread+engine for each window. However, the native window may
        // change in some (quite artificial) cases, e.g. due to a hide -
        // destroy - show on the QWindow.
        bool needsWindow = !engine->window();
        if (engine->window() && engine->window() != wme->window->winId()) {
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - WM_RequestSync - native window handle changes for active engine");
            engine->waitGPU();
            QQuickWindowPrivate::get(wme->window)->cleanupNodesOnShutdown();
            QSGD3D12ShaderEffectNode::cleanupMaterialTypeCache();
            rc->invalidate();
            engine->releaseResources();
            needsWindow = true;
        }
        if (needsWindow) {
            // Must only ever get here when there is no window or releaseResources() has been called.
            const int samples = wme->window->format().samples();
            const bool alpha = wme->window->format().alphaBufferSize() > 0;
            if (Q_UNLIKELY(debug_loop()))
                qDebug() << "RT - WM_RequestSync - initializing D3D12 engine" << wme->window
                         << wme->size << wme->dpr << samples << alpha;
            engine->attachToWindow(wme->window->winId(), wme->size, wme->dpr, samples, alpha);
        }
        exposedWindow = wme->window;
        engine->setWindowSize(wme->size, wme->dpr);
        if (Q_UNLIKELY(debug_loop()))
            qDebug() << "RT - WM_RequestSync" << exposedWindow;
        pendingUpdate |= SyncRequest;
        if (wme->syncInExpose) {
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - WM_RequestSync - triggered from expose");
            pendingUpdate |= ExposeRequest;
        }
        if (wme->forceRenderPass) {
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - WM_RequestSync - repaint regardless");
            pendingUpdate |= RepaintRequest;
        }
        return true;
    }

    case WM_TryRelease: {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - WM_TryRelease");
        mutex.lock();
        renderLoop->lockedForSync = true;
        QSGD3D12TryReleaseEvent *wme = static_cast<QSGD3D12TryReleaseEvent *>(e);
        // Only when no windows are exposed anymore or we are shutting down.
        if (!exposedWindow || wme->destroying) {
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - WM_TryRelease - invalidating rc");
            if (wme->window) {
                QQuickWindowPrivate *wd = QQuickWindowPrivate::get(wme->window);
                if (wme->destroying) {
                    // QSGNode destruction may release graphics resources in use so wait first.
                    engine->waitGPU();
                    // Bye bye nodes...
                    wd->cleanupNodesOnShutdown();
                    QSGD3D12ShaderEffectNode::cleanupMaterialTypeCache();
                }
                rc->invalidate();
                QCoreApplication::processEvents();
                QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
                if (wme->destroying)
                    delete wd->animationController;
            }
            if (wme->destroying)
                active = false;
            if (sleeping)
                stopEventProcessing = true;
        } else {
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - WM_TryRelease - not releasing because window is still active");
        }
        waitCondition.wakeOne();
        renderLoop->lockedForSync = false;
        mutex.unlock();
        return true;
    }

    case WM_Grab: {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - WM_Grab");
        QSGD3D12GrabEvent *wme = static_cast<QSGD3D12GrabEvent *>(e);
        Q_ASSERT(wme->window);
        Q_ASSERT(wme->window == exposedWindow || !exposedWindow);
        mutex.lock();
        if (wme->window) {
            // Grabbing is generally done by rendering a frame and reading the
            // color buffer contents back, without presenting, and then
            // creating a QImage from the returned data. It is terribly
            // inefficient since it involves a full blocking wait for the GPU.
            // However, our hands are tied by the existing, synchronous APIs of
            // QQuickWindow and such.
            QQuickWindowPrivate *wd = QQuickWindowPrivate::get(wme->window);
            rc->initialize(nullptr);
            wd->syncSceneGraph();
            wd->renderSceneGraph(wme->window->size());
            *wme->image = engine->executeAndWaitReadbackRenderTarget();
        }
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - WM_Grab - waking gui to handle result");
        waitCondition.wakeOne();
        mutex.unlock();
        return true;
    }

    case WM_PostJob: {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - WM_PostJob");
        QSGD3D12JobEvent *wme = static_cast<QSGD3D12JobEvent *>(e);
        Q_ASSERT(wme->window == exposedWindow);
        if (exposedWindow) {
            wme->job->run();
            delete wme->job;
            wme->job = nullptr;
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - WM_PostJob - job done");
        }
        return true;
    }

    case WM_RequestRepaint:
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - WM_RequestPaint");
        // When GUI posts this event, it is followed by a polishAndSync, so we
        // must not exit the event loop yet.
        pendingUpdate |= RepaintRequest;
        break;

    default:
        break;
    }

    return QThread::event(e);
}

void QSGD3D12RenderThread::postEvent(QEvent *e)
{
    eventQueue.addEvent(e);
}

void QSGD3D12RenderThread::processEvents()
{
    while (eventQueue.hasMoreEvents()) {
        QEvent *e = eventQueue.takeEvent(false);
        event(e);
        delete e;
    }
}

void QSGD3D12RenderThread::processEventsAndWaitForMore()
{
    stopEventProcessing = false;
    while (!stopEventProcessing) {
        QEvent *e = eventQueue.takeEvent(true);
        event(e);
        delete e;
    }
}

void QSGD3D12RenderThread::run()
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug("RT - run()");

    engine = new QSGD3D12Engine;
    rc->setEngine(engine);

    rtAnim = rc->sceneGraphContext()->createAnimationDriver(nullptr);
    rtAnim->install();

    if (QQmlDebugConnector::service<QQmlProfilerService>())
        QQuickProfiler::registerAnimationCallback();

    while (active) {
        if (exposedWindow)
            syncAndRender();

        processEvents();
        QCoreApplication::processEvents();

        if (pendingUpdate == 0 || !exposedWindow) {
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - done drawing, sleep");
            sleeping = true;
            processEventsAndWaitForMore();
            sleeping = false;
        }
    }

    if (Q_UNLIKELY(debug_loop()))
        qDebug("RT - run() exiting");

    delete rtAnim;
    rtAnim = nullptr;

    rc->moveToThread(renderLoop->thread());
    moveToThread(renderLoop->thread());

    rc->setEngine(nullptr);
    delete engine;
    engine = nullptr;
}

void QSGD3D12RenderThread::sync(bool inExpose)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug("RT - sync");

    mutex.lock();
    Q_ASSERT_X(renderLoop->lockedForSync, "QSGD3D12RenderThread::sync()", "sync triggered with gui not locked");

    // Recover from device loss.
    if (!engine->hasResources()) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - sync - device was lost, resetting scenegraph");
        QQuickWindowPrivate::get(exposedWindow)->cleanupNodesOnShutdown();
        QSGD3D12ShaderEffectNode::cleanupMaterialTypeCache();
        rc->invalidate();
    }

    if (engine->window()) {
        QQuickWindowPrivate *wd = QQuickWindowPrivate::get(exposedWindow);
        bool hadRenderer = wd->renderer != nullptr;
        // If the scene graph was touched since the last sync() make sure it sends the
        // changed signal.
        if (wd->renderer)
            wd->renderer->clearChangedFlag();

        rc->initialize(nullptr);
        wd->syncSceneGraph();

        if (!hadRenderer && wd->renderer) {
            if (Q_UNLIKELY(debug_loop()))
                qDebug("RT - created renderer");
            syncResultedInChanges = true;
            connect(wd->renderer, &QSGRenderer::sceneGraphChanged, this,
                    &QSGD3D12RenderThread::onSceneGraphChanged, Qt::DirectConnection);
        }

        // Process deferred deletes now, directly after the sync as deleteLater
        // on the GUI must now also have resulted in SG changes and the delete
        // is a safe operation.
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    }

    if (!inExpose) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - sync complete, waking gui");
        waitCondition.wakeOne();
        mutex.unlock();
    }
}

void QSGD3D12RenderThread::syncAndRender()
{
    if (Q_UNLIKELY(debug_time())) {
        sinceLastTime = threadTimer.nsecsElapsed();
        threadTimer.start();
    }
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphRenderLoopFrame);

    QElapsedTimer waitTimer;
    waitTimer.start();

    if (Q_UNLIKELY(debug_loop()))
        qDebug("RT - syncAndRender()");

    syncResultedInChanges = false;
    QQuickWindowPrivate *wd = QQuickWindowPrivate::get(exposedWindow);

    const bool repaintRequested = (pendingUpdate & RepaintRequest) || wd->customRenderStage;
    const bool syncRequested = pendingUpdate & SyncRequest;
    const bool exposeRequested = (pendingUpdate & ExposeRequest) == ExposeRequest;
    pendingUpdate = 0;

    if (syncRequested)
        sync(exposeRequested);

#ifndef QSG_NO_RENDER_TIMING
    if (Q_UNLIKELY(debug_time()))
        syncTime = threadTimer.nsecsElapsed();
#endif
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRenderLoopFrame,
                              QQuickProfiler::SceneGraphRenderLoopSync);

    if (!syncResultedInChanges && !repaintRequested) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - no changes, render aborted");
        int waitTime = vsyncDelta - (int) waitTimer.elapsed();
        if (waitTime > 0)
            msleep(waitTime);
        return;
    }

    if (Q_UNLIKELY(debug_loop()))
        qDebug("RT - rendering started");

    if (rtAnim->isRunning()) {
        wd->animationController->lock();
        rtAnim->advance();
        wd->animationController->unlock();
    }

    bool canRender = wd->renderer != nullptr;
    // Recover from device loss.
    if (!engine->hasResources()) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - syncAndRender - device was lost, posting FullUpdateRequest");
        // Cannot do anything here because gui is not locked. Request a new
        // sync+render round on the gui thread and let the sync handle it.
        QCoreApplication::postEvent(exposedWindow, new QEvent(QEvent::Type(QQuickWindowPrivate::FullUpdateRequest)));
        canRender = false;
    }

    if (canRender) {
        wd->renderSceneGraph(engine->windowSize());
        if (Q_UNLIKELY(debug_time()))
            renderTime = threadTimer.nsecsElapsed();
        Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRenderLoopFrame,
                                  QQuickProfiler::SceneGraphRenderLoopRender);

        // The engine is able to have multiple frames in flight. This in effect is
        // similar to BufferQueueingOpenGL. Provide an env var to force the
        // traditional blocking swap behavior, just in case.
        static bool blockOnEachFrame = qEnvironmentVariableIntValue("QT_D3D_BLOCKING_PRESENT") != 0;

        if (!wd->customRenderStage || !wd->customRenderStage->swap())
            engine->present();

        if (blockOnEachFrame)
            engine->waitGPU();

        // The concept of "frame swaps" is quite misleading by default, when
        // blockOnEachFrame is not used, but emit it for compatibility.
        wd->fireFrameSwapped();
    } else {
        Q_QUICK_SG_PROFILE_SKIP(QQuickProfiler::SceneGraphRenderLoopFrame,
                                QQuickProfiler::SceneGraphRenderLoopSync, 1);
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - window not ready, skipping render");
    }

    if (Q_UNLIKELY(debug_loop()))
        qDebug("RT - rendering done");

    if (exposeRequested) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("RT - wake gui after initial expose");
        waitCondition.wakeOne();
        mutex.unlock();
    }

    if (Q_UNLIKELY(debug_time()))
        qDebug("Frame rendered with 'd3d12' renderloop in %dms, sync=%d, render=%d, swap=%d - (on render thread)",
               int(threadTimer.elapsed()),
               int((syncTime/1000000)),
               int((renderTime - syncTime) / 1000000),
               int(threadTimer.elapsed() - renderTime / 1000000));

    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphRenderLoopFrame,
                           QQuickProfiler::SceneGraphRenderLoopSwap);

    static int devLossTest = qEnvironmentVariableIntValue("QT_D3D_TEST_DEVICE_LOSS");
    if (devLossTest > 0) {
        static QElapsedTimer kt;
        static bool timerRunning = false;
        if (!timerRunning) {
            kt.start();
            timerRunning = true;
        } else if (kt.elapsed() > 5000) {
            --devLossTest;
            kt.restart();
            engine->simulateDeviceLoss();
        }
    }
}

template<class T> T *windowFor(const QVector<T> &list, QQuickWindow *window)
{
    for (const T &t : list) {
        if (t.window == window)
            return const_cast<T *>(&t);
    }
    return nullptr;
}

QSGD3D12ThreadedRenderLoop::QSGD3D12ThreadedRenderLoop()
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug("d3d12 THREADED render loop ctor");

    sg = new QSGD3D12Context;

    anim = sg->createAnimationDriver(this);
    connect(anim, &QAnimationDriver::started, this, &QSGD3D12ThreadedRenderLoop::onAnimationStarted);
    connect(anim, &QAnimationDriver::stopped, this, &QSGD3D12ThreadedRenderLoop::onAnimationStopped);
    anim->install();
}

QSGD3D12ThreadedRenderLoop::~QSGD3D12ThreadedRenderLoop()
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug("d3d12 THREADED render loop dtor");

    delete sg;
}

void QSGD3D12ThreadedRenderLoop::show(QQuickWindow *window)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "show" << window;
}

void QSGD3D12ThreadedRenderLoop::hide(QQuickWindow *window)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "hide" << window;

    if (window->isExposed())
        handleObscurity(windowFor(windows, window));

    releaseResources(window);
}

void QSGD3D12ThreadedRenderLoop::resize(QQuickWindow *window)
{
    if (!window->isExposed() || window->size().isEmpty())
        return;

    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "resize" << window << window->size();
}

void QSGD3D12ThreadedRenderLoop::windowDestroyed(QQuickWindow *window)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "window destroyed" << window;

    WindowData *w = windowFor(windows, window);
    if (!w)
        return;

    handleObscurity(w);
    handleResourceRelease(w, true);

    QSGD3D12RenderThread *thread = w->thread;
    while (thread->isRunning())
        QThread::yieldCurrentThread();

    Q_ASSERT(thread->thread() == QThread::currentThread());
    delete thread;

    for (int i = 0; i < windows.size(); ++i) {
        if (windows.at(i).window == window) {
            windows.removeAt(i);
            break;
        }
    }
}

void QSGD3D12ThreadedRenderLoop::exposureChanged(QQuickWindow *window)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "exposure changed" << window;

    if (window->isExposed()) {
        handleExposure(window);
    } else {
        WindowData *w = windowFor(windows, window);
        if (w)
            handleObscurity(w);
    }
}

QImage QSGD3D12ThreadedRenderLoop::grab(QQuickWindow *window)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "grab" << window;

    WindowData *w = windowFor(windows, window);
    // Have to support invisible (but created()'ed) windows as well.
    // Unlike with GL, leaving that case for QQuickWindow to handle is not feasible.
    const bool tempExpose = !w;
    if (tempExpose) {
        handleExposure(window);
        w = windowFor(windows, window);
        Q_ASSERT(w);
    }

    if (!w->thread->isRunning())
        return QImage();

    if (!window->handle())
        window->create();

    QQuickWindowPrivate *wd = QQuickWindowPrivate::get(window);
    wd->polishItems();

    QImage result;
    w->thread->mutex.lock();
    lockedForSync = true;
    w->thread->postEvent(new QSGD3D12GrabEvent(window, &result));
    w->thread->waitCondition.wait(&w->thread->mutex);
    lockedForSync = false;
    w->thread->mutex.unlock();

    result.setDevicePixelRatio(window->effectiveDevicePixelRatio());

    if (tempExpose)
        handleObscurity(w);

    return result;
}

void QSGD3D12ThreadedRenderLoop::update(QQuickWindow *window)
{
    WindowData *w = windowFor(windows, window);
    if (!w)
        return;

    if (w->thread == QThread::currentThread()) {
        w->thread->requestRepaint();
        return;
    }

    // We set forceRenderPass because we want to make sure the QQuickWindow
    // actually does a full render pass after the next sync.
    w->forceRenderPass = true;
    scheduleUpdate(w);
}

void QSGD3D12ThreadedRenderLoop::maybeUpdate(QQuickWindow *window)
{
    WindowData *w = windowFor(windows, window);
    if (w)
        scheduleUpdate(w);
}

// called in response to window->requestUpdate()
void QSGD3D12ThreadedRenderLoop::handleUpdateRequest(QQuickWindow *window)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "handleUpdateRequest" << window;

    WindowData *w = windowFor(windows, window);
    if (w)
        polishAndSync(w, false);
}

QAnimationDriver *QSGD3D12ThreadedRenderLoop::animationDriver() const
{
    return anim;
}

QSGContext *QSGD3D12ThreadedRenderLoop::sceneGraphContext() const
{
    return sg;
}

QSGRenderContext *QSGD3D12ThreadedRenderLoop::createRenderContext(QSGContext *) const
{
    return sg->createRenderContext();
}

void QSGD3D12ThreadedRenderLoop::releaseResources(QQuickWindow *window)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "releaseResources" << window;

    WindowData *w = windowFor(windows, window);
    if (w)
        handleResourceRelease(w, false);
}

void QSGD3D12ThreadedRenderLoop::postJob(QQuickWindow *window, QRunnable *job)
{
    WindowData *w = windowFor(windows, window);
    if (w && w->thread && w->thread->exposedWindow)
        w->thread->postEvent(new QSGD3D12JobEvent(window, job));
    else
        delete job;
}

QSurface::SurfaceType QSGD3D12ThreadedRenderLoop::windowSurfaceType() const
{
    return QSurface::OpenGLSurface;
}

bool QSGD3D12ThreadedRenderLoop::interleaveIncubation() const
{
    bool somethingVisible = false;
    for (const WindowData &w : windows) {
        if (w.window->isVisible() && w.window->isExposed()) {
            somethingVisible = true;
            break;
        }
    }
    return somethingVisible && anim->isRunning();
}

int QSGD3D12ThreadedRenderLoop::flags() const
{
    return SupportsGrabWithoutExpose;
}

bool QSGD3D12ThreadedRenderLoop::event(QEvent *e)
{
    if (e->type() == QEvent::Timer) {
        QTimerEvent *te = static_cast<QTimerEvent *>(e);
        if (te->timerId() == animationTimer) {
            anim->advance();
            emit timeToIncubate();
            return true;
        }
    }

    return QObject::event(e);
}

void QSGD3D12ThreadedRenderLoop::onAnimationStarted()
{
    startOrStopAnimationTimer();

    for (const WindowData &w : qAsConst(windows))
        w.window->requestUpdate();
}

void QSGD3D12ThreadedRenderLoop::onAnimationStopped()
{
    startOrStopAnimationTimer();
}

void QSGD3D12ThreadedRenderLoop::startOrStopAnimationTimer()
{
    int exposedWindowCount = 0;
    const WindowData *exposed = nullptr;

    for (int i = 0; i < windows.size(); ++i) {
        const WindowData &w(windows[i]);
        if (w.window->isVisible() && w.window->isExposed()) {
            ++exposedWindowCount;
            exposed = &w;
        }
    }

    if (animationTimer && (exposedWindowCount == 1 || !anim->isRunning())) {
        killTimer(animationTimer);
        animationTimer = 0;
        // If animations are running, make sure we keep on animating
        if (anim->isRunning())
            exposed->window->requestUpdate();
    } else if (!animationTimer && exposedWindowCount != 1 && anim->isRunning()) {
        animationTimer = startTimer(qsgrl_animation_interval());
    }
}

void QSGD3D12ThreadedRenderLoop::handleExposure(QQuickWindow *window)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "handleExposure" << window;

    WindowData *w = windowFor(windows, window);
    if (!w) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("adding window to list");
        WindowData win;
        win.window = window;
        QSGRenderContext *rc = QQuickWindowPrivate::get(window)->context; // will transfer ownership
        win.thread = new QSGD3D12RenderThread(this, rc);
        win.updateDuringSync = false;
        win.forceRenderPass = true; // also covered by polishAndSync(inExpose=true), but doesn't hurt
        windows.append(win);
        w = &windows.last();
    }

    // set this early as we'll be rendering shortly anyway and this avoids
    // special casing exposure in polishAndSync.
    w->thread->exposedWindow = window;

    if (w->window->size().isEmpty()
        || (w->window->isTopLevel() && !w->window->geometry().intersects(w->window->screen()->availableGeometry()))) {
#ifndef QT_NO_DEBUG
        qWarning().noquote().nospace() << "QSGD3D12ThreadedRenderLoop: expose event received for window "
            << w->window << " with invalid geometry: " << w->window->geometry()
            << " on " << w->window->screen();
#endif
    }

    if (!w->window->handle())
        w->window->create();

    // Start render thread if it is not running
    if (!w->thread->isRunning()) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("starting render thread");
        // Push a few things to the render thread.
        QQuickAnimatorController *controller = QQuickWindowPrivate::get(w->window)->animationController;
        if (controller->thread() != w->thread)
            controller->moveToThread(w->thread);
        if (w->thread->thread() == QThread::currentThread()) {
            w->thread->rc->moveToThread(w->thread);
            w->thread->moveToThread(w->thread);
        }

        w->thread->active = true;
        w->thread->start();

        if (!w->thread->isRunning())
            qFatal("Render thread failed to start, aborting application.");
    }

    polishAndSync(w, true);

    startOrStopAnimationTimer();
}

void QSGD3D12ThreadedRenderLoop::handleObscurity(WindowData *w)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "handleObscurity" << w->window;

    if (w->thread->isRunning()) {
        w->thread->mutex.lock();
        w->thread->postEvent(new QSGD3D12WindowEvent(w->window, WM_Obscure));
        w->thread->waitCondition.wait(&w->thread->mutex);
        w->thread->mutex.unlock();
    }

    startOrStopAnimationTimer();
}

void QSGD3D12ThreadedRenderLoop::scheduleUpdate(WindowData *w)
{
    if (!QCoreApplication::instance())
        return;

    if (!w || !w->thread->isRunning())
        return;

    QThread *current = QThread::currentThread();
    if (current != QCoreApplication::instance()->thread() && (current != w->thread || !lockedForSync)) {
        qWarning() << "Updates can only be scheduled from GUI thread or from QQuickItem::updatePaintNode()";
        return;
    }

    if (current == w->thread) {
        w->updateDuringSync = true;
        return;
    }

    w->window->requestUpdate();
}

void QSGD3D12ThreadedRenderLoop::handleResourceRelease(WindowData *w, bool destroying)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "handleResourceRelease" << (destroying ? "destroying" : "hide/releaseResources") << w->window;

    w->thread->mutex.lock();
    if (w->thread->isRunning() && w->thread->active) {
        QQuickWindow *window = w->window;

        // Note that window->handle() is typically null by this time because
        // the platform window is already destroyed. This should not be a
        // problem for the D3D cleanup.

        w->thread->postEvent(new QSGD3D12TryReleaseEvent(window, destroying));
        w->thread->waitCondition.wait(&w->thread->mutex);

        // Avoid a shutdown race condition.
        // If SG is invalidated and 'active' becomes false, the thread's run()
        // method will exit. handleExposure() relies on QThread::isRunning() (because it
        // potentially needs to start the thread again) and our mutex cannot be used to
        // track the thread stopping, so we wait a few nanoseconds extra so the thread
        // can exit properly.
        if (!w->thread->active)
            w->thread->wait();
    }
    w->thread->mutex.unlock();
}

void QSGD3D12ThreadedRenderLoop::polishAndSync(WindowData *w, bool inExpose)
{
    if (Q_UNLIKELY(debug_loop()))
        qDebug() << "polishAndSync" << (inExpose ? "(in expose)" : "(normal)") << w->window;

    QQuickWindow *window = w->window;
    if (!w->thread || !w->thread->exposedWindow) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("polishAndSync - not exposed, abort");
        return;
    }

    // Flush pending touch events.
    QQuickWindowPrivate::get(window)->flushFrameSynchronousEvents();
    // The delivery of the event might have caused the window to stop rendering
    w = windowFor(windows, window);
    if (!w || !w->thread || !w->thread->exposedWindow) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("polishAndSync - removed after touch event flushing, abort");
        return;
    }

    QElapsedTimer timer;
    qint64 polishTime = 0;
    qint64 waitTime = 0;
    qint64 syncTime = 0;
    if (Q_UNLIKELY(debug_time()))
        timer.start();
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphPolishAndSync);

    QQuickWindowPrivate *wd = QQuickWindowPrivate::get(window);
    wd->polishItems();

    if (Q_UNLIKELY(debug_time()))
        polishTime = timer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphPolishAndSync,
                              QQuickProfiler::SceneGraphPolishAndSyncPolish);

    w->updateDuringSync = false;

    emit window->afterAnimating();

    if (Q_UNLIKELY(debug_loop()))
        qDebug("polishAndSync - lock for sync");
    w->thread->mutex.lock();
    lockedForSync = true;
    w->thread->postEvent(new QSGD3D12SyncEvent(window, inExpose, w->forceRenderPass));
    w->forceRenderPass = false;

    if (Q_UNLIKELY(debug_loop()))
        qDebug("polishAndSync - wait for sync");
    if (Q_UNLIKELY(debug_time()))
        waitTime = timer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphPolishAndSync,
                              QQuickProfiler::SceneGraphPolishAndSyncWait);
    w->thread->waitCondition.wait(&w->thread->mutex);
    lockedForSync = false;
    w->thread->mutex.unlock();
    if (Q_UNLIKELY(debug_loop()))
        qDebug("polishAndSync - unlock after sync");

    if (Q_UNLIKELY(debug_time()))
        syncTime = timer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphPolishAndSync,
                              QQuickProfiler::SceneGraphPolishAndSyncSync);

    if (!animationTimer && anim->isRunning()) {
        if (Q_UNLIKELY(debug_loop()))
            qDebug("polishAndSync - advancing animations");
        anim->advance();
        // We need to trigger another sync to keep animations running...
        w->window->requestUpdate();
        emit timeToIncubate();
    } else if (w->updateDuringSync) {
        w->window->requestUpdate();
    }

    if (Q_UNLIKELY(debug_time()))
        qDebug().nospace()
                << "Frame prepared with 'd3d12' renderloop"
                << ", polish=" << (polishTime / 1000000)
                << ", lock=" << (waitTime - polishTime) / 1000000
                << ", blockedForSync=" << (syncTime - waitTime) / 1000000
                << ", animations=" << (timer.nsecsElapsed() - syncTime) / 1000000
                << " - (on gui thread) " << window;

    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphPolishAndSync,
                           QQuickProfiler::SceneGraphPolishAndSyncAnimations);
}

#include "qsgd3d12threadedrenderloop.moc"

QT_END_NAMESPACE
