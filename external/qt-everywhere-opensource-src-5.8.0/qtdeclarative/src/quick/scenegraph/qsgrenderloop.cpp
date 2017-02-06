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

#include "qsgrenderloop_p.h"
#include "qsgthreadedrenderloop_p.h"
#include "qsgwindowsrenderloop_p.h"
#include <private/qquickanimatorcontroller_p.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTime>
#include <QtCore/QLibraryInfo>
#include <QtCore/private/qabstractanimation_p.h>

#include <QtGui/QOffscreenSurface>
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

#include <QtQml/private/qqmlglobal_p.h>

#include <QtQuick/QQuickWindow>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qsgcontext_p.h>
#include <private/qquickprofiler_p.h>

#ifndef QT_NO_OPENGL
# include <QtGui/QOpenGLContext>
# include <private/qsgdefaultrendercontext_p.h>
#if QT_CONFIG(quick_shadereffect)
# include <private/qquickopenglshadereffectnode_p.h>
#endif
#endif

#ifdef Q_OS_WIN
#  include <QtCore/qt_windows.h>
#endif

QT_BEGIN_NAMESPACE

extern bool qsg_useConsistentTiming();
extern Q_GUI_EXPORT QImage qt_gl_read_framebuffer(const QSize &size, bool alpha_format, bool include_alpha);
#ifndef QT_NO_OPENGL
/*!
    expectations for this manager to work:
     - one opengl context to render multiple windows
     - OpenGL pipeline will not block for vsync in swap
     - OpenGL pipeline will block based on a full buffer queue.
     - Multiple screens can share the OpenGL context
     - Animations are advanced for all windows once per swap
 */

DEFINE_BOOL_CONFIG_OPTION(qmlNoThreadedRenderer, QML_BAD_GUI_RENDER_LOOP);
DEFINE_BOOL_CONFIG_OPTION(qmlForceThreadedRenderer, QML_FORCE_THREADED_RENDERER); // Might trigger graphics driver threading bugs, use at own risk
#endif
QSGRenderLoop *QSGRenderLoop::s_instance = 0;

QSGRenderLoop::~QSGRenderLoop()
{
}

QSurface::SurfaceType QSGRenderLoop::windowSurfaceType() const
{
    return QSurface::OpenGLSurface;
}

void QSGRenderLoop::cleanup()
{
    if (!s_instance)
        return;
    for (QQuickWindow *w : s_instance->windows()) {
        QQuickWindowPrivate *wd = QQuickWindowPrivate::get(w);
        if (wd->windowManager == s_instance) {
           s_instance->windowDestroyed(w);
           wd->windowManager = 0;
        }
    }
    delete s_instance;
    s_instance = 0;
}

/*!
 * Non-threaded render loops immediately run the job if there is a context.
 */
void QSGRenderLoop::postJob(QQuickWindow *window, QRunnable *job)
{
    Q_ASSERT(job);
#ifndef QT_NO_OPENGL
    Q_ASSERT(window);
    if (window->openglContext()) {
        window->openglContext()->makeCurrent(window);
        job->run();
    }
#else
    Q_UNUSED(window)
    job->run();
#endif
    delete job;
}
#ifndef QT_NO_OPENGL
class QSGGuiThreadRenderLoop : public QSGRenderLoop
{
    Q_OBJECT
public:
    QSGGuiThreadRenderLoop();
    ~QSGGuiThreadRenderLoop();

    void show(QQuickWindow *window);
    void hide(QQuickWindow *window);

    void windowDestroyed(QQuickWindow *window);

    void renderWindow(QQuickWindow *window);
    void exposureChanged(QQuickWindow *window);
    QImage grab(QQuickWindow *window);

    void maybeUpdate(QQuickWindow *window);
    void update(QQuickWindow *window) { maybeUpdate(window); } // identical for this implementation.
    void handleUpdateRequest(QQuickWindow *);

    void releaseResources(QQuickWindow *) { }

    QAnimationDriver *animationDriver() const { return 0; }

    QSGContext *sceneGraphContext() const;
    QSGRenderContext *createRenderContext(QSGContext *) const { return rc; }

    struct WindowData {
        bool updatePending : 1;
        bool grabOnly : 1;
    };

    QHash<QQuickWindow *, WindowData> m_windows;

    QOpenGLContext *gl;
    QSGContext *sg;
    QSGRenderContext *rc;

    QImage grabContent;
};
#endif
QSGRenderLoop *QSGRenderLoop::instance()
{
    if (!s_instance) {

        // For compatibility with 5.3 and earlier's QSG_INFO environment variables
        if (qEnvironmentVariableIsSet("QSG_INFO"))
            const_cast<QLoggingCategory &>(QSG_LOG_INFO()).setEnabled(QtDebugMsg, true);

        s_instance = QSGContext::createWindowManager();
#ifndef QT_NO_OPENGL
        if (!s_instance) {

            enum RenderLoopType {
                BasicRenderLoop,
                ThreadedRenderLoop,
                WindowsRenderLoop
            };

            RenderLoopType loopType = BasicRenderLoop;

#ifdef Q_OS_WIN
            // With desktop OpenGL (opengl32.dll), use threaded. Otherwise (ANGLE) use windows.
            if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL
                && QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::ThreadedOpenGL))
                loopType = ThreadedRenderLoop;
            else
                loopType = WindowsRenderLoop;
#else
            if (QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::ThreadedOpenGL))
                loopType = ThreadedRenderLoop;
#endif
            if (qmlNoThreadedRenderer())
                loopType = BasicRenderLoop;
            else if (qmlForceThreadedRenderer())
                loopType = ThreadedRenderLoop;

            if (Q_UNLIKELY(qEnvironmentVariableIsSet("QSG_RENDER_LOOP"))) {
                const QByteArray loopName = qgetenv("QSG_RENDER_LOOP");
                if (loopName == QByteArrayLiteral("windows"))
                    loopType = WindowsRenderLoop;
                else if (loopName == QByteArrayLiteral("basic"))
                    loopType = BasicRenderLoop;
                else if (loopName == QByteArrayLiteral("threaded"))
                    loopType = ThreadedRenderLoop;
            }

            switch (loopType) {
            case ThreadedRenderLoop:
                qCDebug(QSG_LOG_INFO, "threaded render loop");
                s_instance = new QSGThreadedRenderLoop();
                break;
            case WindowsRenderLoop:
                qCDebug(QSG_LOG_INFO, "windows render loop");
                s_instance = new QSGWindowsRenderLoop();
                break;
            default:
                qCDebug(QSG_LOG_INFO, "QSG: basic render loop");
                s_instance = new QSGGuiThreadRenderLoop();
                break;
            }
        }
#endif
        qAddPostRoutine(QSGRenderLoop::cleanup);
    }

    return s_instance;
}

void QSGRenderLoop::setInstance(QSGRenderLoop *instance)
{
    Q_ASSERT(!s_instance);
    s_instance = instance;
}

void QSGRenderLoop::handleContextCreationFailure(QQuickWindow *window,
                                                 bool isEs)
{
    QString translatedMessage;
    QString untranslatedMessage;
    QQuickWindowPrivate::contextCreationFailureMessage(window->requestedFormat(),
                                                       &translatedMessage,
                                                       &untranslatedMessage,
                                                       isEs);
    // If there is a slot connected to the error signal, emit it and leave it to
    // the application to do something with the message. If nothing is connected,
    // show a message on our own and terminate.
    const bool signalEmitted =
        QQuickWindowPrivate::get(window)->emitError(QQuickWindow::ContextNotAvailable,
                                                    translatedMessage);
#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
    if (!signalEmitted && !QLibraryInfo::isDebugBuild() && !GetConsoleWindow()) {
        MessageBox(0, (LPCTSTR) translatedMessage.utf16(),
                   (LPCTSTR)(QCoreApplication::applicationName().utf16()),
                   MB_OK | MB_ICONERROR);
    }
#endif // Q_OS_WIN && !Q_OS_WINRT
    if (!signalEmitted)
        qFatal("%s", qPrintable(untranslatedMessage));
}
#ifndef QT_NO_OPENGL
QSGGuiThreadRenderLoop::QSGGuiThreadRenderLoop()
    : gl(0)
{
    if (qsg_useConsistentTiming()) {
        QUnifiedTimer::instance(true)->setConsistentTiming(true);
        qCDebug(QSG_LOG_INFO, "using fixed animation steps");
    }
    sg = QSGContext::createDefaultContext();
    rc = sg->createRenderContext();
}

QSGGuiThreadRenderLoop::~QSGGuiThreadRenderLoop()
{
    delete rc;
    delete sg;
}

void QSGGuiThreadRenderLoop::show(QQuickWindow *window)
{
    WindowData data;
    data.updatePending = false;
    data.grabOnly = false;
    m_windows[window] = data;

    maybeUpdate(window);
}

void QSGGuiThreadRenderLoop::hide(QQuickWindow *window)
{
    QQuickWindowPrivate *cd = QQuickWindowPrivate::get(window);
    cd->fireAboutToStop();
}

void QSGGuiThreadRenderLoop::windowDestroyed(QQuickWindow *window)
{
    m_windows.remove(window);
    hide(window);
    QQuickWindowPrivate *d = QQuickWindowPrivate::get(window);

    bool current = false;
    QScopedPointer<QOffscreenSurface> offscreenSurface;
    if (gl) {
        QSurface *surface = window;
        // There may be no platform window if the window got closed.
        if (!window->handle()) {
            offscreenSurface.reset(new QOffscreenSurface);
            offscreenSurface->setFormat(gl->format());
            offscreenSurface->create();
            surface = offscreenSurface.data();
        }
        current = gl->makeCurrent(surface);
    }
    if (Q_UNLIKELY(!current))
        qCDebug(QSG_LOG_RENDERLOOP) << "cleanup without an OpenGL context";

#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
    QQuickOpenGLShaderEffectMaterial::cleanupMaterialCache();
#endif

    d->cleanupNodesOnShutdown();
    if (m_windows.size() == 0) {
        rc->invalidate();
        delete gl;
        gl = 0;
    } else if (gl && window == gl->surface() && current) {
        gl->doneCurrent();
    }

    delete d->animationController;
}

void QSGGuiThreadRenderLoop::renderWindow(QQuickWindow *window)
{
    QQuickWindowPrivate *cd = QQuickWindowPrivate::get(window);
    if (!cd->isRenderable() || !m_windows.contains(window))
        return;

    WindowData &data = const_cast<WindowData &>(m_windows[window]);

    bool current = false;

    if (!gl) {
        gl = new QOpenGLContext();
        gl->setFormat(window->requestedFormat());
        gl->setScreen(window->screen());
        if (qt_gl_global_share_context())
            gl->setShareContext(qt_gl_global_share_context());
        if (!gl->create()) {
            const bool isEs = gl->isOpenGLES();
            delete gl;
            gl = 0;
            handleContextCreationFailure(window, isEs);
        } else {
            cd->fireOpenGLContextCreated(gl);
            current = gl->makeCurrent(window);
        }
        if (current) {
            auto openglRenderContext = static_cast<QSGDefaultRenderContext *>(cd->context);
            openglRenderContext->initialize(gl);
        }
    } else {
        current = gl->makeCurrent(window);
    }

    bool alsoSwap = data.updatePending;
    data.updatePending = false;

    if (!current)
        return;

    if (!data.grabOnly) {
        cd->flushFrameSynchronousEvents();
        // Event delivery/processing triggered the window to be deleted or stop rendering.
        if (!m_windows.contains(window))
            return;
    }
    QElapsedTimer renderTimer;
    qint64 renderTime = 0, syncTime = 0, polishTime = 0;
    bool profileFrames = QSG_LOG_TIME_RENDERLOOP().isDebugEnabled();
    if (profileFrames)
        renderTimer.start();
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphPolishFrame);

    cd->polishItems();

    if (profileFrames)
        polishTime = renderTimer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_SWITCH(QQuickProfiler::SceneGraphPolishFrame,
                              QQuickProfiler::SceneGraphRenderLoopFrame,
                              QQuickProfiler::SceneGraphPolishPolish);

    emit window->afterAnimating();

    cd->syncSceneGraph();

    if (profileFrames)
        syncTime = renderTimer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRenderLoopFrame,
                              QQuickProfiler::SceneGraphRenderLoopSync);

    cd->renderSceneGraph(window->size());

    if (profileFrames)
        renderTime = renderTimer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphRenderLoopFrame,
                              QQuickProfiler::SceneGraphRenderLoopRender);

    if (data.grabOnly) {
        bool alpha = window->format().alphaBufferSize() > 0 && window->color().alpha() != 255;
        grabContent = qt_gl_read_framebuffer(window->size() * window->effectiveDevicePixelRatio(), alpha, alpha);
        data.grabOnly = false;
    }

    if (alsoSwap && window->isVisible()) {
        if (!cd->customRenderStage || !cd->customRenderStage->swap())
            gl->swapBuffers(window);
        cd->fireFrameSwapped();
    }

    qint64 swapTime = 0;
    if (profileFrames)
        swapTime = renderTimer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_END(QQuickProfiler::SceneGraphRenderLoopFrame,
                           QQuickProfiler::SceneGraphRenderLoopSwap);

    if (QSG_LOG_TIME_RENDERLOOP().isDebugEnabled()) {
        static QTime lastFrameTime = QTime::currentTime();
        qCDebug(QSG_LOG_TIME_RENDERLOOP,
                "Frame rendered with 'basic' renderloop in %dms, polish=%d, sync=%d, render=%d, swap=%d, frameDelta=%d",
                int(swapTime / 1000000),
                int(polishTime / 1000000),
                int((syncTime - polishTime) / 1000000),
                int((renderTime - syncTime) / 1000000),
                int((swapTime - renderTime) / 10000000),
                int(lastFrameTime.msecsTo(QTime::currentTime())));
        lastFrameTime = QTime::currentTime();
    }

    // Might have been set during syncSceneGraph()
    if (data.updatePending)
        maybeUpdate(window);
}

void QSGGuiThreadRenderLoop::exposureChanged(QQuickWindow *window)
{
    if (window->isExposed()) {
        m_windows[window].updatePending = true;
        renderWindow(window);
    }
}

QImage QSGGuiThreadRenderLoop::grab(QQuickWindow *window)
{
    if (!m_windows.contains(window))
        return QImage();

    m_windows[window].grabOnly = true;

    renderWindow(window);

    QImage grabbed = grabContent;
    grabContent = QImage();
    return grabbed;
}

void QSGGuiThreadRenderLoop::maybeUpdate(QQuickWindow *window)
{
    if (!m_windows.contains(window))
        return;

    m_windows[window].updatePending = true;
    window->requestUpdate();
}

QSGContext *QSGGuiThreadRenderLoop::sceneGraphContext() const
{
    return sg;
}

void QSGGuiThreadRenderLoop::handleUpdateRequest(QQuickWindow *window)
{
    renderWindow(window);
}

#endif

#include "qsgrenderloop.moc"

QT_END_NAMESPACE
