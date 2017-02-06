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

#include "qsgsoftwarethreadedrenderloop_p.h"
#include "qsgsoftwarecontext_p.h"
#include "qsgsoftwarerenderer_p.h"

#include <private/qsgrenderer_p.h>
#include <private/qquickwindow_p.h>
#include <private/qquickprofiler_p.h>
#include <private/qquickanimatorcontroller_p.h>
#include <private/qquickprofiler_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qqmldebugconnector_p.h>

#include <qpa/qplatformbackingstore.h>

#include <QtCore/QQueue>
#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtGui/QGuiApplication>
#include <QtGui/QBackingStore>
#include <QtQuick/QQuickWindow>

QT_BEGIN_NAMESPACE

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

class QSGSoftwareWindowEvent : public QEvent
{
public:
    QSGSoftwareWindowEvent(QQuickWindow *c, QEvent::Type type) : QEvent(type), window(c) { }
    QQuickWindow *window;
};

class QSGSoftwareTryReleaseEvent : public QSGSoftwareWindowEvent
{
public:
    QSGSoftwareTryReleaseEvent(QQuickWindow *win, bool destroy)
        : QSGSoftwareWindowEvent(win, WM_TryRelease), destroying(destroy) { }
    bool destroying;
};

class QSGSoftwareSyncEvent : public QSGSoftwareWindowEvent
{
public:
    QSGSoftwareSyncEvent(QQuickWindow *c, bool inExpose, bool force)
        : QSGSoftwareWindowEvent(c, WM_RequestSync)
        , size(c->size())
        , dpr(c->effectiveDevicePixelRatio())
        , syncInExpose(inExpose)
        , forceRenderPass(force) { }
    QSize size;
    float dpr;
    bool syncInExpose;
    bool forceRenderPass;
};

class QSGSoftwareGrabEvent : public QSGSoftwareWindowEvent
{
public:
    QSGSoftwareGrabEvent(QQuickWindow *c, QImage *result)
        : QSGSoftwareWindowEvent(c, WM_Grab), image(result) { }
    QImage *image;
};

class QSGSoftwareJobEvent : public QSGSoftwareWindowEvent
{
public:
    QSGSoftwareJobEvent(QQuickWindow *c, QRunnable *postedJob)
        : QSGSoftwareWindowEvent(c, WM_PostJob), job(postedJob) { }
    ~QSGSoftwareJobEvent() { delete job; }
    QRunnable *job;
};

class QSGSoftwareEventQueue : public QQueue<QEvent *>
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

class QSGSoftwareRenderThread : public QThread
{
    Q_OBJECT
public:
    QSGSoftwareRenderThread(QSGSoftwareThreadedRenderLoop *rl, QSGRenderContext *renderContext)
        : renderLoop(rl)
    {
        rc = static_cast<QSGSoftwareRenderContext *>(renderContext);
        vsyncDelta = qsgrl_animation_interval();
    }

    ~QSGSoftwareRenderThread()
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

    QSGSoftwareThreadedRenderLoop *renderLoop;
    QSGSoftwareRenderContext *rc;
    QAnimationDriver *rtAnim = nullptr;
    volatile bool active = false;
    uint pendingUpdate = 0;
    bool sleeping = false;
    bool syncResultedInChanges = false;
    float vsyncDelta;
    QMutex mutex;
    QWaitCondition waitCondition;
    QQuickWindow *exposedWindow = nullptr;
    QBackingStore *backingStore = nullptr;
    bool stopEventProcessing = false;
    QSGSoftwareEventQueue eventQueue;
    QElapsedTimer renderThrottleTimer;
    qint64 syncTime;
    qint64 renderTime;
    qint64 sinceLastTime;

public slots:
    void onSceneGraphChanged() {
        syncResultedInChanges = true;
    }
};

bool QSGSoftwareRenderThread::event(QEvent *e)
{
    switch ((int)e->type()) {

    case WM_Obscure:
        Q_ASSERT(!exposedWindow || exposedWindow == static_cast<QSGSoftwareWindowEvent *>(e)->window);
        qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "RT - WM_Obscure" << exposedWindow;
        mutex.lock();
        if (exposedWindow) {
            QQuickWindowPrivate::get(exposedWindow)->fireAboutToStop();
            qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_Obscure - window removed");
            exposedWindow = nullptr;
            delete backingStore;
            backingStore = nullptr;
        }
        waitCondition.wakeOne();
        mutex.unlock();
        return true;

    case WM_RequestSync: {
        QSGSoftwareSyncEvent *wme = static_cast<QSGSoftwareSyncEvent *>(e);
        if (sleeping)
            stopEventProcessing = true;
        exposedWindow = wme->window;
        if (backingStore == nullptr)
            backingStore = new QBackingStore(exposedWindow);
        if (backingStore->size() != exposedWindow->size())
            backingStore->resize(exposedWindow->size());
        qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "RT - WM_RequestSync" << exposedWindow;
        pendingUpdate |= SyncRequest;
        if (wme->syncInExpose) {
            qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_RequestSync - triggered from expose");
            pendingUpdate |= ExposeRequest;
        }
        if (wme->forceRenderPass) {
            qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_RequestSync - repaint regardless");
            pendingUpdate |= RepaintRequest;
        }
        return true;
    }

    case WM_TryRelease: {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_TryRelease");
        mutex.lock();
        renderLoop->lockedForSync = true;
        QSGSoftwareTryReleaseEvent *wme = static_cast<QSGSoftwareTryReleaseEvent *>(e);
        // Only when no windows are exposed anymore or we are shutting down.
        if (!exposedWindow || wme->destroying) {
            qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_TryRelease - invalidating rc");
            if (wme->window) {
                QQuickWindowPrivate *wd = QQuickWindowPrivate::get(wme->window);
                if (wme->destroying) {
                    // Bye bye nodes...
                    wd->cleanupNodesOnShutdown();
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
            qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_TryRelease - not releasing because window is still active");
        }
        waitCondition.wakeOne();
        renderLoop->lockedForSync = false;
        mutex.unlock();
        return true;
    }

    case WM_Grab: {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_Grab");
        QSGSoftwareGrabEvent *wme = static_cast<QSGSoftwareGrabEvent *>(e);
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
            auto softwareRenderer = static_cast<QSGSoftwareRenderer*>(wd->renderer);
            if (softwareRenderer)
                softwareRenderer->setBackingStore(backingStore);
            rc->initialize(nullptr);
            wd->syncSceneGraph();
            wd->renderSceneGraph(wme->window->size());
            *wme->image = backingStore->handle()->toImage();
        }
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_Grab - waking gui to handle result");
        waitCondition.wakeOne();
        mutex.unlock();
        return true;
    }

    case WM_PostJob: {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_PostJob");
        QSGSoftwareJobEvent *wme = static_cast<QSGSoftwareJobEvent *>(e);
        Q_ASSERT(wme->window == exposedWindow);
        if (exposedWindow) {
            wme->job->run();
            delete wme->job;
            wme->job = nullptr;
            qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_PostJob - job done");
        }
        return true;
    }

    case WM_RequestRepaint:
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - WM_RequestPaint");
        // When GUI posts this event, it is followed by a polishAndSync, so we
        // must not exit the event loop yet.
        pendingUpdate |= RepaintRequest;
        break;

    default:
        break;
    }

    return QThread::event(e);
}

void QSGSoftwareRenderThread::postEvent(QEvent *e)
{
    eventQueue.addEvent(e);
}

void QSGSoftwareRenderThread::processEvents()
{
    while (eventQueue.hasMoreEvents()) {
        QEvent *e = eventQueue.takeEvent(false);
        event(e);
        delete e;
    }
}

void QSGSoftwareRenderThread::processEventsAndWaitForMore()
{
    stopEventProcessing = false;
    while (!stopEventProcessing) {
        QEvent *e = eventQueue.takeEvent(true);
        event(e);
        delete e;
    }
}

void QSGSoftwareRenderThread::run()
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - run()");

    rtAnim = rc->sceneGraphContext()->createAnimationDriver(nullptr);
    rtAnim->install();

    if (QQmlDebugConnector::service<QQmlProfilerService>())
        QQuickProfiler::registerAnimationCallback();

    renderThrottleTimer.start();

    while (active) {
        if (exposedWindow)
            syncAndRender();

        processEvents();
        QCoreApplication::processEvents();

        if (pendingUpdate == 0 || !exposedWindow) {
            qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - done drawing, sleep");
            sleeping = true;
            processEventsAndWaitForMore();
            sleeping = false;
        }
    }

    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - run() exiting");

    delete rtAnim;
    rtAnim = nullptr;

    rc->moveToThread(renderLoop->thread());
    moveToThread(renderLoop->thread());
}

void QSGSoftwareRenderThread::sync(bool inExpose)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - sync");

    mutex.lock();
    Q_ASSERT_X(renderLoop->lockedForSync, "QSGD3D12RenderThread::sync()", "sync triggered with gui not locked");

    if (exposedWindow) {
        QQuickWindowPrivate *wd = QQuickWindowPrivate::get(exposedWindow);
        bool hadRenderer = wd->renderer != nullptr;
        // If the scene graph was touched since the last sync() make sure it sends the
        // changed signal.
        if (wd->renderer)
            wd->renderer->clearChangedFlag();

        rc->initialize(nullptr);
        wd->syncSceneGraph();

        if (!hadRenderer && wd->renderer) {
            qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - created renderer");
            syncResultedInChanges = true;
            connect(wd->renderer, &QSGRenderer::sceneGraphChanged, this,
                    &QSGSoftwareRenderThread::onSceneGraphChanged, Qt::DirectConnection);
        }

        // Process deferred deletes now, directly after the sync as deleteLater
        // on the GUI must now also have resulted in SG changes and the delete
        // is a safe operation.
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    }

    if (!inExpose) {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - sync complete, waking gui");
        waitCondition.wakeOne();
        mutex.unlock();
    }
}

void QSGSoftwareRenderThread::syncAndRender()
{
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphRenderLoopFrame);

    QElapsedTimer waitTimer;
    waitTimer.start();

    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - syncAndRender()");

    syncResultedInChanges = false;
    QQuickWindowPrivate *wd = QQuickWindowPrivate::get(exposedWindow);

    const bool repaintRequested = (pendingUpdate & RepaintRequest) || wd->customRenderStage;
    const bool syncRequested = pendingUpdate & SyncRequest;
    const bool exposeRequested = (pendingUpdate & ExposeRequest) == ExposeRequest;
    pendingUpdate = 0;

    if (syncRequested)
        sync(exposeRequested);

    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRenderLoopFrame,
                              QQuickProfiler::SceneGraphRenderLoopSync);

    if (!syncResultedInChanges && !repaintRequested) {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - no changes, render aborted");
        int waitTime = vsyncDelta - (int) waitTimer.elapsed();
        if (waitTime > 0)
            msleep(waitTime);
        return;
    }

    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - rendering started");

    if (rtAnim->isRunning()) {
        wd->animationController->lock();
        rtAnim->advance();
        wd->animationController->unlock();
    }

    bool canRender = wd->renderer != nullptr;

    if (canRender) {
        auto softwareRenderer = static_cast<QSGSoftwareRenderer*>(wd->renderer);
        if (softwareRenderer)
            softwareRenderer->setBackingStore(backingStore);
        wd->renderSceneGraph(exposedWindow->size());

        Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRenderLoopFrame,
                                  QQuickProfiler::SceneGraphRenderLoopRender);

        if (softwareRenderer && (!wd->customRenderStage || !wd->customRenderStage->swap()))
            backingStore->flush(softwareRenderer->flushRegion());

        // Since there is no V-Sync with QBackingStore, throttle rendering the refresh
        // rate of the current screen the window is on.
        int blockTime = vsyncDelta - (int) renderThrottleTimer.elapsed();
        if (blockTime > 0) {
            qCDebug(QSG_RASTER_LOG_RENDERLOOP) <<  "RT - blocking for " << blockTime << "ms";
            msleep(blockTime);
        }
        renderThrottleTimer.restart();

        wd->fireFrameSwapped();
    } else {
        Q_QUICK_SG_PROFILE_SKIP(QQuickProfiler::SceneGraphRenderLoopFrame,
                                QQuickProfiler::SceneGraphRenderLoopSync, 1);
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - window not ready, skipping render");
    }

    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - rendering done");

    if (exposeRequested) {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "RT - wake gui after initial expose");
        waitCondition.wakeOne();
        mutex.unlock();
    }

    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphRenderLoopFrame,
                           QQuickProfiler::SceneGraphRenderLoopSwap);
}

template<class T> T *windowFor(const QVector<T> &list, QQuickWindow *window)
{
    for (const T &t : list) {
        if (t.window == window)
            return const_cast<T *>(&t);
    }
    return nullptr;
}


QSGSoftwareThreadedRenderLoop::QSGSoftwareThreadedRenderLoop()
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "software threaded render loop constructor");
    m_sg = new QSGSoftwareContext;
    m_anim = m_sg->createAnimationDriver(this);
    connect(m_anim, &QAnimationDriver::started, this, &QSGSoftwareThreadedRenderLoop::onAnimationStarted);
    connect(m_anim, &QAnimationDriver::stopped, this, &QSGSoftwareThreadedRenderLoop::onAnimationStopped);
    m_anim->install();
}

QSGSoftwareThreadedRenderLoop::~QSGSoftwareThreadedRenderLoop()
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "software threaded render loop destructor");
    delete m_sg;
}

void QSGSoftwareThreadedRenderLoop::show(QQuickWindow *window)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "show" << window;
}

void QSGSoftwareThreadedRenderLoop::hide(QQuickWindow *window)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "hide" << window;

    if (window->isExposed())
        handleObscurity(windowFor(m_windows, window));

    releaseResources(window);
}

void QSGSoftwareThreadedRenderLoop::resize(QQuickWindow *window)
{
    if (!window->isExposed() || window->size().isEmpty())
        return;

    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "resize" << window << window->size();
}

void QSGSoftwareThreadedRenderLoop::windowDestroyed(QQuickWindow *window)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "window destroyed" << window;

    WindowData *w = windowFor(m_windows, window);
    if (!w)
        return;

    handleObscurity(w);
    handleResourceRelease(w, true);

    QSGSoftwareRenderThread *thread = w->thread;
    while (thread->isRunning())
        QThread::yieldCurrentThread();

    Q_ASSERT(thread->thread() == QThread::currentThread());
    delete thread;

    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows.at(i).window == window) {
            m_windows.removeAt(i);
            break;
        }
    }
}

void QSGSoftwareThreadedRenderLoop::exposureChanged(QQuickWindow *window)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "exposure changed" << window;

    if (window->isExposed()) {
        handleExposure(window);
    } else {
        WindowData *w = windowFor(m_windows, window);
        if (w)
            handleObscurity(w);
    }
}

QImage QSGSoftwareThreadedRenderLoop::grab(QQuickWindow *window)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "grab" << window;

    WindowData *w = windowFor(m_windows, window);
    // Have to support invisible (but created()'ed) windows as well.
    // Unlike with GL, leaving that case for QQuickWindow to handle is not feasible.
    const bool tempExpose = !w;
    if (tempExpose) {
        handleExposure(window);
        w = windowFor(m_windows, window);
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
    w->thread->postEvent(new QSGSoftwareGrabEvent(window, &result));
    w->thread->waitCondition.wait(&w->thread->mutex);
    lockedForSync = false;
    w->thread->mutex.unlock();

    result.setDevicePixelRatio(window->effectiveDevicePixelRatio());

    if (tempExpose)
        handleObscurity(w);

    return result;
}

void QSGSoftwareThreadedRenderLoop::update(QQuickWindow *window)
{
    WindowData *w = windowFor(m_windows, window);
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

void QSGSoftwareThreadedRenderLoop::maybeUpdate(QQuickWindow *window)
{
    WindowData *w = windowFor(m_windows, window);
    if (w)
        scheduleUpdate(w);
}

void QSGSoftwareThreadedRenderLoop::handleUpdateRequest(QQuickWindow *window)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "handleUpdateRequest" << window;

    WindowData *w = windowFor(m_windows, window);
    if (w)
        polishAndSync(w, false);
}

QAnimationDriver *QSGSoftwareThreadedRenderLoop::animationDriver() const
{
    return m_anim;
}

QSGContext *QSGSoftwareThreadedRenderLoop::sceneGraphContext() const
{
    return m_sg;
}

QSGRenderContext *QSGSoftwareThreadedRenderLoop::createRenderContext(QSGContext *) const
{
    return m_sg->createRenderContext();
}

void QSGSoftwareThreadedRenderLoop::releaseResources(QQuickWindow *window)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "releaseResources" << window;

    WindowData *w = windowFor(m_windows, window);
    if (w)
        handleResourceRelease(w, false);
}

void QSGSoftwareThreadedRenderLoop::postJob(QQuickWindow *window, QRunnable *job)
{
    WindowData *w = windowFor(m_windows, window);
    if (w && w->thread && w->thread->exposedWindow)
        w->thread->postEvent(new QSGSoftwareJobEvent(window, job));
    else
        delete job;
}

QSurface::SurfaceType QSGSoftwareThreadedRenderLoop::windowSurfaceType() const
{
    return QSurface::RasterSurface;
}

bool QSGSoftwareThreadedRenderLoop::interleaveIncubation() const
{
    bool somethingVisible = false;
    for (const WindowData &w : m_windows) {
        if (w.window->isVisible() && w.window->isExposed()) {
            somethingVisible = true;
            break;
        }
    }
    return somethingVisible && m_anim->isRunning();
}

int QSGSoftwareThreadedRenderLoop::flags() const
{
    return SupportsGrabWithoutExpose;
}

bool QSGSoftwareThreadedRenderLoop::event(QEvent *e)
{
    if (e->type() == QEvent::Timer) {
        QTimerEvent *te = static_cast<QTimerEvent *>(e);
        if (te->timerId() == animationTimer) {
            m_anim->advance();
            emit timeToIncubate();
            return true;
        }
    }

    return QObject::event(e);
}

void QSGSoftwareThreadedRenderLoop::onAnimationStarted()
{
    startOrStopAnimationTimer();

    for (const WindowData &w : qAsConst(m_windows))
        w.window->requestUpdate();
}

void QSGSoftwareThreadedRenderLoop::onAnimationStopped()
{
    startOrStopAnimationTimer();
}

void QSGSoftwareThreadedRenderLoop::startOrStopAnimationTimer()
{
    int exposedWindowCount = 0;
    const WindowData *exposed = nullptr;

    for (int i = 0; i < m_windows.size(); ++i) {
        const WindowData &w(m_windows[i]);
        if (w.window->isVisible() && w.window->isExposed()) {
            ++exposedWindowCount;
            exposed = &w;
        }
    }

    if (animationTimer && (exposedWindowCount == 1 || !m_anim->isRunning())) {
        killTimer(animationTimer);
        animationTimer = 0;
        // If animations are running, make sure we keep on animating
        if (m_anim->isRunning())
            exposed->window->requestUpdate();
    } else if (!animationTimer && exposedWindowCount != 1 && m_anim->isRunning()) {
        animationTimer = startTimer(qsgrl_animation_interval());
    }
}

void QSGSoftwareThreadedRenderLoop::handleExposure(QQuickWindow *window)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "handleExposure" << window;

    WindowData *w = windowFor(m_windows, window);
    if (!w) {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "adding window to list");
        WindowData win;
        win.window = window;
        QSGRenderContext *rc = QQuickWindowPrivate::get(window)->context; // will transfer ownership
        win.thread = new QSGSoftwareRenderThread(this, rc);
        win.updateDuringSync = false;
        win.forceRenderPass = true; // also covered by polishAndSync(inExpose=true), but doesn't hurt
        m_windows.append(win);
        w = &m_windows.last();
    }

    // set this early as we'll be rendering shortly anyway and this avoids
    // special casing exposure in polishAndSync.
    w->thread->exposedWindow = window;

    if (w->window->size().isEmpty()
        || (w->window->isTopLevel() && !w->window->geometry().intersects(w->window->screen()->availableGeometry()))) {
#ifndef QT_NO_DEBUG
        qWarning().noquote().nospace() << "QSGSotwareThreadedRenderLoop: expose event received for window "
            << w->window << " with invalid geometry: " << w->window->geometry()
            << " on " << w->window->screen();
#endif
    }

    if (!w->window->handle())
        w->window->create();

    // Start render thread if it is not running
    if (!w->thread->isRunning()) {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "starting render thread");
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

void QSGSoftwareThreadedRenderLoop::handleObscurity(QSGSoftwareThreadedRenderLoop::WindowData *w)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "handleObscurity" << w->window;

    if (w->thread->isRunning()) {
        w->thread->mutex.lock();
        w->thread->postEvent(new QSGSoftwareWindowEvent(w->window, WM_Obscure));
        w->thread->waitCondition.wait(&w->thread->mutex);
        w->thread->mutex.unlock();
    }

    startOrStopAnimationTimer();
}

void QSGSoftwareThreadedRenderLoop::scheduleUpdate(QSGSoftwareThreadedRenderLoop::WindowData *w)
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

void QSGSoftwareThreadedRenderLoop::handleResourceRelease(QSGSoftwareThreadedRenderLoop::WindowData *w, bool destroying)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "handleResourceRelease" << (destroying ? "destroying" : "hide/releaseResources") << w->window;

    w->thread->mutex.lock();
    if (w->thread->isRunning() && w->thread->active) {
        QQuickWindow *window = w->window;

        // Note that window->handle() is typically null by this time because
        // the platform window is already destroyed. This should not be a
        // problem for the D3D cleanup.

        w->thread->postEvent(new QSGSoftwareTryReleaseEvent(window, destroying));
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

void QSGSoftwareThreadedRenderLoop::polishAndSync(QSGSoftwareThreadedRenderLoop::WindowData *w, bool inExpose)
{
    qCDebug(QSG_RASTER_LOG_RENDERLOOP) << "polishAndSync" << (inExpose ? "(in expose)" : "(normal)") << w->window;

    QQuickWindow *window = w->window;
    if (!w->thread || !w->thread->exposedWindow) {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "polishAndSync - not exposed, abort");
        return;
    }

    // Flush pending touch events.
    QQuickWindowPrivate::get(window)->flushFrameSynchronousEvents();
    // The delivery of the event might have caused the window to stop rendering
    w = windowFor(m_windows, window);
    if (!w || !w->thread || !w->thread->exposedWindow) {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "polishAndSync - removed after touch event flushing, abort");
        return;
    }

    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphPolishAndSync);

    QQuickWindowPrivate *wd = QQuickWindowPrivate::get(window);
    wd->polishItems();

    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphPolishAndSync,
                              QQuickProfiler::SceneGraphPolishAndSyncPolish);

    w->updateDuringSync = false;

    emit window->afterAnimating();

    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "polishAndSync - lock for sync");
    w->thread->mutex.lock();
    lockedForSync = true;
    w->thread->postEvent(new QSGSoftwareSyncEvent(window, inExpose, w->forceRenderPass));
    w->forceRenderPass = false;

    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "polishAndSync - wait for sync");

    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphPolishAndSync,
                              QQuickProfiler::SceneGraphPolishAndSyncWait);
    w->thread->waitCondition.wait(&w->thread->mutex);
    lockedForSync = false;
    w->thread->mutex.unlock();
    qCDebug(QSG_RASTER_LOG_RENDERLOOP, "polishAndSync - unlock after sync");

    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphPolishAndSync,
                              QQuickProfiler::SceneGraphPolishAndSyncSync);

    if (!animationTimer && m_anim->isRunning()) {
        qCDebug(QSG_RASTER_LOG_RENDERLOOP, "polishAndSync - advancing animations");
        m_anim->advance();
        // We need to trigger another sync to keep animations running...
        w->window->requestUpdate();
        emit timeToIncubate();
    } else if (w->updateDuringSync) {
        w->window->requestUpdate();
    }

    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphPolishAndSync,
                           QQuickProfiler::SceneGraphPolishAndSyncAnimations);
}

#include "qsgsoftwarethreadedrenderloop.moc"

QT_END_NAMESPACE
