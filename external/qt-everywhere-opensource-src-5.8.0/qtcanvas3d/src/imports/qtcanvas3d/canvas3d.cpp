/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "canvas3d_p.h"
#include "context3d_p.h"
#include "canvas3dcommon_p.h"
#include "canvasrendernode_p.h"
#include "teximage3d_p.h"
#include "glcommandqueue_p.h"
#include "canvasglstatedump_p.h"
#include "renderjob_p.h"
#include "canvasrenderer_p.h"
#include "openglversionchecker_p.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtCore/QElapsedTimer>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(canvas3dinfo, "qt.canvas3d.info")
Q_LOGGING_CATEGORY(canvas3drendering, "qt.canvas3d.rendering")
Q_LOGGING_CATEGORY(canvas3dglerrors, "qt.canvas3d.glerrors")

/*!
 * \qmltype Canvas3D
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Canvas that provides a 3D rendering context.
 *
 * The Canvas3D is a QML element that, when placed in your Qt Quick 2 scene, allows you to
 * get a 3D rendering context and call 3D rendering API calls through that context object.
 * Use of the rendering API requires knowledge of OpenGL-like rendering APIs.
 *
 * There are two functions that are called by the Canvas3D implementation:
 * \list
 * \li initializeGL is emitted before the first frame is rendered, and usually during that you get
 * the 3D context and initialize resources to be used later on during the rendering cycle.
 * \li paintGL is emitted for each frame to be rendered, and usually during that you
 * submit 3D rendering calls to draw whatever 3D content you want to be displayed.
 * \endlist
 *
 * \sa Context3D
 */

Canvas::Canvas(QQuickItem *parent):
    QQuickItem(parent),
    m_isNeedRenderQueued(false),
    m_rendererReady(false),
    m_fboSize(0, 0),
    m_maxSize(0, 0),
    m_frameTimeMs(0),
    m_frameSetupTimeMs(0),
    m_maxSamples(0),
    m_devicePixelRatio(1.0f),
    m_isOpenGLES2(false),
    m_isCombinedDepthStencilSupported(false),
    m_isSoftwareRendered(false),
    m_isContextAttribsSet(false),
    m_alphaChanged(false),
    m_resizeGLQueued(false),
    m_allowRenderTargetChange(true),
    m_renderTargetSyncConnected(false),
    m_renderTarget(RenderTargetOffscreenBuffer),
    m_renderOnDemand(false),
    m_renderer(0),
    m_maxVertexAttribs(0),
    m_contextVersion(0),
    m_fps(0),
    m_contextState(ContextNone)
{
    connect(this, &QQuickItem::windowChanged, this, &Canvas::handleWindowChanged);
    connect(this, &Canvas::needRender, this, &Canvas::queueNextRender, Qt::QueuedConnection);
    connect(this, &QQuickItem::widthChanged, this, &Canvas::queueResizeGL, Qt::DirectConnection);
    connect(this, &QQuickItem::heightChanged, this, &Canvas::queueResizeGL, Qt::DirectConnection);
    setAntialiasing(false);

    // Set contents to false in case we are in qml designer to make component look nice
    m_runningInDesigner = QGuiApplication::applicationDisplayName() == "Qml2Puppet";
    setFlag(ItemHasContents, !(m_runningInDesigner || m_renderTarget != RenderTargetOffscreenBuffer));

    OpenGLVersionChecker checker;
    m_isSoftwareRendered = checker.isSoftwareRenderer();
}

/*!
 * \qmlsignal void Canvas3D::initializeGL()
 * Emitted once when Canvas3D is ready and OpenGL state initialization can be done by the client.
 */

/*!
 * \qmlsignal void Canvas3D::paintGL()
 * Emitted each time a new frame should be drawn to Canvas3D.
 * Driven by the Qt Quick scenegraph loop.
 */

/*!
 * \qmlsignal void Canvas3D::contextLost()
 * Emitted when OpenGL context is lost. This happens whenever the parent window of the Canvas3D
 * is destroyed (or otherwise loses its context), or Canvas3D is moved to a different window.
 * Removing Canvas3D from a window and adding it back to the same window doesn't cause context
 * loss, as long as the window itself stays alive.
 *
 * When context is lost, all objects created by Context3D are invalidated.
 *
 * \sa contextRestored
 */

/*!
 * \qmlsignal void Canvas3D::contextRestored()
 * Emitted when OpenGL context is restored after a loss of context occurred. The Context3D attached
 * to the canvas needs to be reinitialized, so initializeGL is also emitted after this signal.
 *
 * \sa contextLost
 */

Canvas::~Canvas()
{
    // Ensure that all JS objects have been destroyed before we destroy the command queue.
    if (!m_context3D.isNull())
        delete m_context3D.data();

    if (m_renderer)
        m_renderer->destroy();
}

/*!
 * \qmlproperty RenderTarget Canvas3D::renderTarget
 * Specifies how the rendering should be done.
 * \list
 * \li \c Canvas3D.RenderTargetOffscreenBuffer indicates rendering is done into an offscreen
 * buffer and the finished texture is used for the Canvas3D item. This is the default target.
 * \li \c Canvas3D.RenderTargetBackground indicates the rendering is done to the background of the
 * Qt Quick scene, in response to QQuickWindow::beforeRendering() signal.
 * \li \c Canvas3D.RenderTargetForeground indicates the rendering is done to the foreground of the
 * Qt Quick scene, in response to QQuickWindow::afterRendering() signal.
 * \endlist
 *
 * \c Canvas3D.RenderTargetBackground and \c Canvas3D.RenderTargetForeground targets render directly
 * to the same framebuffer the rest of the Qt Quick scene uses. This will improve performance
 * on platforms that are fill-rate limited, but using these targets imposes several limitations
 * on the usage of Canvas3D:
 *
 * \list
 * \li Synchronous Context3D commands are not supported outside
 * \l{Canvas3D::initializeGL}{Canvas3D.initializeGL()} signal handler when rendering directly
 * to Qt Quick scene framebuffer, as they cause portions of the command queue to be executed
 * outside the normal frame render sequence, which interferes with the frame clearing logic.
 * Using them will usually result in Canvas3D content not rendering properly.
 * A synchronous command is any Context3D command that requires waiting for Context3D command queue
 * to finish executing before it returns, such as \l{Context3D::getError}{Context3D.getError()},
 * \l{Context3D::finish}{Context3D.finish()},
 * or \l{Context3D::readPixels}{Context3D.readPixels()}. When in doubt, see the individual command
 * documentation to see if that command is synchronous.
 * If your application requires synchronous commands outside
 * \l{Canvas3D::initializeGL}{Canvas3D.initializeGL()} signal handler, you should use
 * \c{Canvas3D.RenderTargetOffscreenBuffer} render target.
 *
 * \li Only Canvas3D items that fill the entire window are supported. Note that you can still
 * control the actual rendering area by using an appropriate viewport.
 *
 * \li The default framebuffer is automatically cleared by Canvas3D every time before the Qt Quick
 * scene renders a frame, even if there are no Context3D commands queued for that frame.
 * This requires Canvas3D to store the commands used to draw the previous frame in case the window
 * is updated by some other component than Canvas3D and use those commands to render the Canvas3D
 * content for frames that do not have fresh content. Only commands issued inside
 * \l{Canvas3D::paintGL}{Canvas3D.paintGL()} signal handler are stored this way.
 * You need to make sure that the content of your \l{Canvas3D::paintGL}{Canvas3D.paintGL()}
 * signal handler is implemented so that it is safe to execute its commands repeatedly.
 * Mainly this means making sure you don't use any synchronous commands or commands
 * that create new persistent OpenGL resources there.
 *
 * \li Issuing Context3D commands outside \l{Canvas3D::paintGL}{Canvas3D.paintGL()} and
 * \l{Canvas3D::initializeGL}{Canvas3D.initializeGL()} signal handlers can in some cases cause
 * unwanted flickering of Canvas3D content, particularly if on-demand rendering is used.
 * It is recommended to avoid issuing any Context3D commands outside these two signal handlers.
 *
 * \li When drawing to the foreground, you should never issue a
 * \l{Context3D::clear}{Context3D.clear(Context3D.GL_COLOR_BUFFER_BIT)} command targeting the
 * default framebuffer, as that will clear all other Qt Quick items from the scene.
 * Clearing depth and stencil buffers is allowed.
 *
 * \li Antialiasing is only supported if the surface format of the window supports multisampling.
 * You may need to specify the surface format of the window explicitly in your \c{main.cpp}.
 *
 * \li You lose the ability to control the z-order of the Canvas3D item itself, as it is always
 * drawn either behind or in front of all other Qt Quick items.
 *
 * \li The context attributes given as Canvas3D.getContext() parameters are ignored and the
 * corresponding values of the Qt Quick context are used.
 *
 * \li Drawing to the background or the foreground doesn't work when Qt Quick is using OpenGL
 * core profile, as Canvas3D requires either OpenGL 2.x compatibility or OpenGL ES2.
 * \endlist
 *
 * This property can only be modified before the Canvas3D item has been rendered for the first time.
 */
void Canvas::setRenderTarget(RenderTarget target)
{
    if (m_allowRenderTargetChange) {
        RenderTarget oldTarget = m_renderTarget;
        m_renderTarget = target;
        if (m_renderTarget == RenderTargetOffscreenBuffer)
            setFlag(ItemHasContents, true);
        else
            setFlag(ItemHasContents, false);
        if (oldTarget != m_renderTarget)
            emit renderTargetChanged();
        if (!m_renderTargetSyncConnected && window()
                && m_renderTarget != RenderTargetOffscreenBuffer) {
            m_renderTargetSyncConnected = true;
            connect(window(), &QQuickWindow::beforeSynchronizing,
                    this, &Canvas::handleBeforeSynchronizing, Qt::DirectConnection);
            window()->setClearBeforeRendering(false);
        }
    } else {
        qCWarning(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                               << ": renderTarget property can only be "
                                               << "modified before Canvas3D item is rendered the "
                                               << "first time";
    }
}

Canvas::RenderTarget Canvas::renderTarget() const
{
    return m_renderTarget;
}

/*!
 * \qmlproperty bool Canvas3D::renderOnDemand
 * If the value is \c{false}, the render loop runs constantly and
 * \l{Canvas3D::paintGL}{Canvas3D.paintGL()} signal is emitted once per frame.
 * If the value is \c{true}, \l{Canvas3D::paintGL}{Canvas3D.paintGL()} is only emitted when
 * Canvas3D content needs to be re-rendered because a geometry change or some other event
 * affecting the Canvas3D content occurred.
 * The application can also request a render using
 * \l{Canvas3D::requestRender}{Canvas3D.requestRender()} method.
 */
void Canvas::setRenderOnDemand(bool enable)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "(" << enable << ")";
    if (enable != m_renderOnDemand) {
        m_renderOnDemand = enable;
        if (m_renderOnDemand)
            handleRendererFpsChange(0);
        else
            emitNeedRender();
        emit renderOnDemandChanged();
    }
}

bool Canvas::renderOnDemand() const
{
    return m_renderOnDemand;
}

/*!
 * \qmlproperty float Canvas3D::devicePixelRatio
 * Specifies the ratio between logical pixels (used by the Qt Quick) and actual physical
 * on-screen pixels (used by the 3D rendering).
 */
float Canvas::devicePixelRatio()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";
    QQuickWindow *win = window();
    if (win)
        return win->devicePixelRatio();
    else
        return 1.0f;
}


/*!
 * \qmlmethod Context3D Canvas3D::getContext(string type)
 * Returns the 3D rendering context that allows 3D rendering calls to be made.
 * The \a type parameter is ignored for now, but a string is expected to be given.
 */
QJSValue Canvas::getContext(const QString &type)
{
    QVariantMap map;
    return getContext(type, map);
}

/*!
 * \qmlmethod Context3D Canvas3D::getContext(string type, Canvas3DContextAttributes options)
 * Returns the 3D rendering context that allows 3D rendering calls to be made.
 * The \a type parameter is ignored for now, but a string is expected to be given.
 * If Canvas3D.renderTarget property value is either \c Canvas3D.RenderTargetBackground or
 * \c Canvas3D.RenderTargetForeground, the \a options parameter is also ignored,
 * the context attributes of the Qt Quick context are used, and the
 * \l{Canvas3DContextAttributes::preserveDrawingBuffer}{Canvas3DContextAttributes.preserveDrawingBuffer}
 * property is forced to \c{false}.
 * The \a options parameter is only parsed when the first call to getContext() is
 * made and is ignored in subsequent calls if given. If the first call is made without
 * giving the \a options parameter, then the context and render target is initialized with
 * default configuration.
 *
 * \sa Canvas3DContextAttributes, Context3D, renderTarget
 */
QJSValue Canvas::getContext(const QString &type, const QVariantMap &options)
{
    Q_UNUSED(type);

    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "(type:" << type
                                         << ", options:" << options
                                         << ")";

    if (!m_isContextAttribsSet) {
        // Accept passed attributes only from first call and ignore for subsequent calls
        m_isContextAttribsSet = true;
        m_alphaChanged = true;
        m_contextAttribs.setFrom(options);

        qCDebug(canvas3drendering).nospace()  << "Canvas3D::" << __FUNCTION__
                                              << " Context attribs:" << m_contextAttribs;

        // If we can't do antialiasing, ensure we don't even try to enable it
        if (m_maxSamples == 0 || m_isSoftwareRendered)
            m_contextAttribs.setAntialias(false);

        // Reflect the fact that creation of stencil attachment
        // causes the creation of depth attachment as well
        if (m_contextAttribs.stencil())
            m_contextAttribs.setDepth(true);

        // Ensure ignored attributes are left to their default state
        m_contextAttribs.setPreferLowPowerToHighPerformance(false);
        m_contextAttribs.setFailIfMajorPerformanceCaveat(false);
    }

    if (!m_renderer->contextCreated()) {
        updateWindowParameters();

        if (!m_renderer->createContext(window(), m_contextAttribs, m_maxVertexAttribs, m_maxSize,
                                       m_contextVersion, m_extensions,
                                       m_isCombinedDepthStencilSupported)) {
            return QJSValue(QJSValue::NullValue);
        }

        setPixelSize(m_renderer->fboSize());
    }

    if (!m_context3D) {
        m_context3D = new CanvasContext(QQmlEngine::contextForObject(this)->engine(),
                                        m_isOpenGLES2, m_maxVertexAttribs,
                                        m_contextVersion, m_extensions,
                                        m_renderer->commandQueue(),
                                        m_isCombinedDepthStencilSupported);

        connect(m_renderer, &CanvasRenderer::textureIdResolved,
                m_context3D.data(), &CanvasContext::handleTextureIdResolved,
                Qt::QueuedConnection);

        m_context3D->setCanvas(this);
        m_context3D->setDevicePixelRatio(m_devicePixelRatio);
        m_context3D->setContextAttributes(m_contextAttribs);

        emit contextChanged(m_context3D.data());
    }

    return QQmlEngine::contextForObject(this)->engine()->newQObject(m_context3D.data());
}

/*!
 * \qmlproperty size Canvas3D::pixelSize
 * Specifies the size of the render target surface in physical on-screen pixels used by
 * the 3D rendering.
 */
QSize Canvas::pixelSize()
{
    return m_fboSize;
}

void Canvas::setPixelSize(QSize pixelSize)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "(pixelSize:" << pixelSize
                                         << ")";
    if (pixelSize.width() > m_maxSize.width()) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << "():"
                                             << "Maximum pixel width exceeded limiting to "
                                             << m_maxSize.width();
        pixelSize.setWidth(m_maxSize.width());
    }

    if (pixelSize.height() > m_maxSize.height()) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << "():"
                                             << "Maximum pixel height exceeded limiting to "
                                             << m_maxSize.height();
        pixelSize.setHeight(m_maxSize.height());
    }

    if (pixelSize.width() <= 0)
        pixelSize.setWidth(1);
    if (pixelSize.height() <= 0)
        pixelSize.setHeight(1);

    if (m_fboSize == pixelSize)
        return;

    m_fboSize = pixelSize;

    // Queue the pixel size signal to next repaint cycle and queue repaint
    queueResizeGL();
    emitNeedRender();
    emit pixelSizeChanged(pixelSize);
}

void Canvas::handleWindowChanged(QQuickWindow *window)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "(" << window << ")";

    if (!window) {
        if (!m_contextWindow.isNull()) {
            if (m_renderTarget != RenderTargetOffscreenBuffer) {
                disconnect(m_contextWindow.data(), &QQuickWindow::beforeSynchronizing,
                           this, &Canvas::handleBeforeSynchronizing);
            }
            if (m_renderer) {
                if (m_renderTarget == RenderTargetForeground) {
                    disconnect(m_contextWindow.data(), &QQuickWindow::beforeRendering,
                            m_renderer, &CanvasRenderer::clearBackground);
                    disconnect(m_contextWindow.data(), &QQuickWindow::afterRendering,
                            m_renderer, &CanvasRenderer::render);
                } else {
                    disconnect(m_contextWindow.data(), &QQuickWindow::beforeRendering,
                            m_renderer, &CanvasRenderer::render);
                }
            }
        }
        return;
    }

    if (window != m_contextWindow.data()) {
        handleContextLost();
        m_contextWindow = window;
    } else {
        // Re-added to same window
        if (!m_context3D.isNull())
            m_context3D->markQuickTexturesDirty();

        if (m_renderer) {
            if (m_renderTarget == RenderTargetForeground) {
                connect(window, &QQuickWindow::beforeRendering,
                        m_renderer, &CanvasRenderer::clearBackground, Qt::DirectConnection);
                connect(window, &QQuickWindow::afterRendering,
                        m_renderer, &CanvasRenderer::render, Qt::DirectConnection);
            } else {
                connect(window, &QQuickWindow::beforeRendering,
                        m_renderer, &CanvasRenderer::render, Qt::DirectConnection);
            }
        }
    }

    if ((!m_allowRenderTargetChange || !m_renderTargetSyncConnected)
            && m_renderTarget != RenderTargetOffscreenBuffer) {
        m_renderTargetSyncConnected = true;
        connect(window, &QQuickWindow::beforeSynchronizing,
                this, &Canvas::handleBeforeSynchronizing, Qt::DirectConnection);
        window->setClearBeforeRendering(false);
    }

    emitNeedRender();
}

void Canvas::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "(newGeometry:" << newGeometry
                                         << ", oldGeometry" << oldGeometry
                                         << ")";
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    emitNeedRender();
}

void Canvas::itemChange(ItemChange change, const ItemChangeData &value)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "(change:" << change
                                         << ")";
    QQuickItem::itemChange(change, value);

    emitNeedRender();
}

/*!
 * \qmlproperty Context3D Canvas3D::context
 * This property can be used to access the context created with getContext() method.
 *
 * \sa getContext()
 */
CanvasContext *Canvas::context()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";
    return m_context3D.data();
}

void Canvas::updateWindowParameters()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    // Update the device pixel ratio
    QQuickWindow *win = window();

    if (win) {
        qreal pixelRatio = win->devicePixelRatio();
        if (pixelRatio != m_devicePixelRatio) {
            m_devicePixelRatio = pixelRatio;
            emit devicePixelRatioChanged(pixelRatio);
            queueResizeGL();
            win->update();
        }
    }

    if (!m_context3D.isNull()) {
        if (m_context3D->devicePixelRatio() != m_devicePixelRatio)
            m_context3D->setDevicePixelRatio(m_devicePixelRatio);
    }
}

void Canvas::sync()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    m_renderer->setFboSize(m_fboSize);

    m_frameTimeMs = uint(m_renderer->previousFrameTime());

    // Update execution queue (GUI thread is locked here)
    m_renderer->transferCommands();

    // Start queuing up another frame
    if (!m_renderOnDemand)
        emitNeedRender();
}

bool Canvas::firstSync()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    if (m_contextState == ContextLost || !m_renderer) {
        if (m_renderer)
            m_renderer->destroy();

        m_renderer = new CanvasRenderer();
        m_contextState = ContextRestoring;

        // Update necessary things to m_context3D
        if (!m_context3D.isNull()) {
           m_context3D->setCommandQueue(m_renderer->commandQueue());
           connect(m_renderer, &CanvasRenderer::textureIdResolved,
                   m_context3D.data(), &CanvasContext::handleTextureIdResolved,
                   Qt::QueuedConnection);
        }
        connect(m_renderer, &CanvasRenderer::fpsChanged,
                this, &Canvas::handleRendererFpsChange);
    }

    if (!m_renderer->qtContextResolved()) {
        m_allowRenderTargetChange = false;
        QSize initializedSize = boundingRect().size().toSize();
        if (initializedSize.width() <= 0)
            initializedSize.setWidth(1);
        if (initializedSize.height() <= 0)
            initializedSize.setHeight(1);
        m_renderer->resolveQtContext(window(), initializedSize, m_renderTarget);
        m_isOpenGLES2 = m_renderer->isOpenGLES2();

        if (m_renderTarget != RenderTargetOffscreenBuffer) {
            m_renderer->getQtContextAttributes(m_contextAttribs);
            m_isContextAttribsSet = true;
            m_renderer->init(window(), m_contextAttribs, m_maxVertexAttribs, m_maxSize,
                             m_contextVersion, m_extensions, m_isCombinedDepthStencilSupported);
            setPixelSize(m_renderer->fboSize());
        } else {
            m_renderer->createContextShare();
            m_maxSamples = m_renderer->maxSamples();
        }

        connect(window(), &QQuickWindow::sceneGraphInvalidated,
                m_renderer, &CanvasRenderer::shutDown, Qt::DirectConnection);
        connect(window(), &QQuickWindow::sceneGraphInvalidated,
                this, &Canvas::handleContextLost, Qt::QueuedConnection);
        connect(window(), &QObject::destroyed, this, &Canvas::handleContextLost);

        if (m_renderTarget == RenderTargetForeground) {
            connect(window(), &QQuickWindow::beforeRendering,
                    m_renderer, &CanvasRenderer::clearBackground, Qt::DirectConnection);
            connect(window(), &QQuickWindow::afterRendering,
                    m_renderer, &CanvasRenderer::render, Qt::DirectConnection);
        } else {
            connect(window(), &QQuickWindow::beforeRendering,
                    m_renderer, &CanvasRenderer::render, Qt::DirectConnection);
        }

        return true;
    }
    return false;
}

CanvasRenderer *Canvas::renderer()
{
    return m_renderer;
}

QSGNode *Canvas::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "("
                                         << oldNode <<", " << data
                                         << ")";
    updateWindowParameters();
    QSize initializedSize = boundingRect().size().toSize();
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " size:" << initializedSize
                                         << " devicePixelRatio:" << m_devicePixelRatio;
    if (m_runningInDesigner
            || initializedSize.width() < 0
            || initializedSize.height() < 0
            || !window()) {
        delete oldNode;
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Returns null";

        m_rendererReady = false;
        return 0;
    }

    CanvasRenderNode *node = static_cast<CanvasRenderNode *>(oldNode);

    if (firstSync()) {
        update();
        return 0;
    }

    if (!node) {
        node = new CanvasRenderNode(window());

        /* Set up connections to get the production of FBO textures in sync with vsync on the
         * main thread.
         *
         * When the OpenGL commands for rendering the new texture are queued in queueNextRender(),
         * QQuickItem::update() is called to trigger updatePaintNode() call (this function).
         * QQuickWindow::update() is also queued to actually cause the redraw to happen.
         *
         * The queued OpenGL commands are transferred into the execution queue below. This is safe
         * because GUI thread is blocked at this point. After we have transferred the commands
         * for execution, we emit needRender() signal to trigger queueing of the commands for
         * the next frame.
         *
         * The queued commands are actually executed in the render thread in response to
         * QQuickWindow::beforeRendering() signal, which is connected to
         * CanvasRenderer::render() slot.
         *
         * When executing commands, an internalTextureComplete command indicates a complete frame.
         * The render buffers are swapped at that point and the node texture is updated via direct
         * connected CanvasRenderer::textureReady() signal.
         *
         * This rendering pipeline is throttled by vsync on the scene graph rendering thread.
         */
        connect(m_renderer, &CanvasRenderer::textureReady,
                node, &CanvasRenderNode::newTexture,
                Qt::DirectConnection);

        m_rendererReady = true;
        emitNeedRender();
    }

    if (m_alphaChanged) {
        node->setAlpha(m_contextAttribs.alpha());
        m_alphaChanged = false;
    }

    sync();

    node->setRect(boundingRect());

    return node;
}

/*!
 * \qmlproperty int Canvas3D::fps
 * This property specifies the current number of frames rendered per second.
 * The value is recalculated every 500 ms, as long as any rendering is done.
 *
 * \note This property only gets updated after a Canvas3D frame is rendered, so if no frames
 * are being drawn, this property value won't change.
 * It is also based on the number of Canvas3D frames actually rendered since the value was
 * last updated, so it may not accurately reflect the actual rendering performance when
 * If \l{Canvas3D::renderOnDemand}{Canvas3D.renderOnDemand} property is \c{true}.
 *
 * \sa frameTimeMs
 */
uint Canvas::fps()
{
    return m_fps;
}

/*!
   \qmlmethod int Canvas3D::frameTimeMs()

   This method returns the number of milliseconds the renderer took to process the OpenGL portion
   of the rendering for the previous frame.
   Before any frames have been rendered this method returns 0.
   This time is measured from the point OpenGL commands are transferred to render thread to the time
   glFinish() returns, so it doesn't include the time spent parsing JavaScript, nor the time
   the scene graph takes to present the frame to the screen.
   This value is updated for the previous frame when the next frame OpenGL command transfer is done.

   \sa fps, frameSetupTimeMs
*/
int Canvas::frameTimeMs()
{
    return int(m_frameTimeMs);
}

/*!
   \qmlmethod int Canvas3D::frameSetupTimeMs()
   \since QtCanvas3D 1.1

   This method returns the number of milliseconds Canvas3D took to process the PaintGL signal
   for the previous frame. Before any frames have been rendered this method returns 0.
   This time doesn't include time spent on actual OpenGL rendering of the frame, nor the time
   the scene graph takes to present the frame to the screen.
   This value is updated after PaintGL signal handler returns.

   \sa fps, frameTimeMs
*/
int Canvas::frameSetupTimeMs()
{
    return int(m_frameSetupTimeMs);
}

void Canvas::queueNextRender()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    m_isNeedRenderQueued = false;

    updateWindowParameters();

    // Don't try to do anything before the renderer/node are ready
    if (!m_rendererReady || !window()) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Renderer or window not ready, returning";
        return;
    }

    if (m_context3D.isNull() || m_contextState == ContextRestoring) {
        // Call the initialize function from QML/JavaScript. It'll call the getContext()
        // that in turn creates the renderer context.
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Emit initializeGL() signal";

        if (!m_context3D.isNull()) {
            m_context3D->setContextLostState(false);
            emit contextRestored();
        }

        // Call init on JavaScript side to queue the user's GL initialization commands.
        // The initial context creation get will also initialize the context command queue.
        emit initializeGL();

        m_contextState = ContextAlive;

        if (!m_isContextAttribsSet) {
            qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                 << " Context attributes not set, returning";
            return;
        }

        if (!m_renderer->contextCreated()) {
            qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                 << " QOpenGLContext not created, returning";
            return;
        }
    }

    // Signal changes in pixel size
    if (m_resizeGLQueued) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Emit resizeGL() signal";
        emit resizeGL(int(width()), int(height()), m_devicePixelRatio);
        m_resizeGLQueued = false;
    }

    // Check if any images are loaded and need to be notified
    QQmlEngine *engine = QQmlEngine::contextForObject(this)->engine();
    CanvasTextureImageFactory::factory(engine)->notifyLoadedImages();

    // Call render in QML JavaScript side to queue the user's GL rendering commands.
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " Emit paintGL() signal";

    if (m_renderTarget != RenderTargetOffscreenBuffer)
        m_renderer->commandQueue()->queueCommand(CanvasGlCommandQueue::internalBeginPaint);

    m_frameTimer.start();
    emit paintGL();
    m_frameSetupTimeMs = m_frameTimer.elapsed();

    // Indicate texture completion point by queueing internalTextureComplete command
    m_renderer->commandQueue()->queueCommand(CanvasGlCommandQueue::internalTextureComplete);

    if (m_renderTarget == RenderTargetOffscreenBuffer) {
        // Trigger updatePaintNode() and actual frame draw
        update();
    }
    window()->update();
}

void Canvas::queueResizeGL()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    m_resizeGLQueued = true;
}

/*!
 * \qmlmethod void Canvas3D::requestRender()
 * Queues a new frame for rendering when \l{Canvas3D::renderOnDemand}{Canvas3D.renderOnDemand}
 * property is \c{true}.
 * Does nothing when \l{Canvas3D::renderOnDemand}{Canvas3D.renderOnDemand} property is \c{false}.
 */
void Canvas::requestRender()
{
    if (m_renderOnDemand)
        emitNeedRender();
}

void Canvas::emitNeedRender()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    if (m_isNeedRenderQueued) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " needRender already queued, returning";
        return;
    }

    m_isNeedRenderQueued = true;
    emit needRender();
}

void Canvas::handleBeforeSynchronizing()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    updateWindowParameters();

    if (firstSync()) {
        m_rendererReady = true;
        emitNeedRender();
        return;
    }

    sync();
}

void Canvas::handleRendererFpsChange(uint fps)
{
    if (fps != m_fps) {
        m_fps = fps;
        emit fpsChanged(m_fps);
    }
}

void Canvas::handleContextLost()
{
    if (m_contextState == ContextAlive || m_contextState == ContextRestoring) {
        m_contextState = ContextLost;
        m_rendererReady = false;
        m_fboSize = QSize(0, 0);

        if (!m_contextWindow.isNull()) {
            disconnect(m_contextWindow.data(), &QQuickWindow::sceneGraphInvalidated,
                       this, &Canvas::handleContextLost);
            disconnect(m_contextWindow.data(), &QObject::destroyed,
                        this, &Canvas::handleContextLost);
        }

        if (!m_context3D.isNull())
            m_context3D->setContextLostState(true);

        emit contextLost();
    }
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
