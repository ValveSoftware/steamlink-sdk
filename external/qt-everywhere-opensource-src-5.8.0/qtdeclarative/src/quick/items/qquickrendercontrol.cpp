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

#include "qquickrendercontrol.h"
#include "qquickrendercontrol_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTime>
#include <QtQuick/private/qquickanimatorcontroller_p.h>

#ifndef QT_NO_OPENGL
# include <QtGui/QOpenGLContext>
# include <QtQuick/private/qsgdefaultrendercontext_p.h>
#if QT_CONFIG(quick_shadereffect)
# include <QtQuick/private/qquickopenglshadereffectnode_p.h>
#endif
#endif
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

#include <QtQml/private/qqmlglobal_p.h>

#include <QtQuick/QQuickWindow>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qsgsoftwarerenderer_p.h>
#include <QtCore/private/qobject_p.h>

QT_BEGIN_NAMESPACE
#ifndef QT_NO_OPENGL
extern Q_GUI_EXPORT QImage qt_gl_read_framebuffer(const QSize &size, bool alpha_format, bool include_alpha);
#endif
/*!
  \class QQuickRenderControl

  \brief The QQuickRenderControl class provides a mechanism for rendering the Qt
  Quick scenegraph onto an offscreen render target in a fully
  application-controlled manner.

  \since 5.4

  QQuickWindow and QQuickView and their associated internal render loops render
  the Qt Quick scene onto a native window. In some cases, for example when
  integrating with 3rd party OpenGL renderers, it might be beneficial to get the
  scene into a texture that can then be used in arbitrary ways by the external
  rendering engine. QQuickRenderControl makes this possible in a hardware
  accelerated manner, unlike the performance-wise limited alternative of using
  QQuickWindow::grabWindow()

  When using a QQuickRenderControl, the QQuickWindow does not have to be shown
  or even created at all. This means there will not be an underlying native
  window for it. Instead, the QQuickWindow instance is associated with the
  render control, using the overload of the QQuickWindow constructor, and an
  OpenGL framebuffer object by calling QQuickWindow::setRenderTarget().

  Management of the context and framebuffer object is up to the application. The
  context that will be used by Qt Quick must be created before calling
  initialize(). The creation of the framebuffer object can be deferred, see
  below. Qt 5.4 introduces the ability for QOpenGLContext to adopt existing
  native contexts. Together with QQuickRenderControl this makes it possible to
  create a QOpenGLContext that shares with an external rendering engine's
  existing context. This new QOpenGLContext can then be used to render the Qt
  Quick scene into a texture that is accessible by the other engine's context
  too.

  Loading and instantiation of the QML components happen by using a
  QQmlEngine. Once the root object is created, it will need to be parented to
  the QQuickWindow's contentItem().

  Applications will usually have to connect to 4 important signals:

  \list

  \li QQuickWindow::sceneGraphInitialized() Emitted at some point after calling
  QQuickRenderControl::initialize(). Upon this signal, the application is
  expected to create its framebuffer object and associate it with the
  QQuickWindow.

  \li QQuickWindow::sceneGraphInvalidated() When the scenegraph resources are
  released, the framebuffer object can be destroyed too.

  \li QQuickRenderControl::renderRequested() Indicates that the scene has to be
  rendered by calling render(). After making the context current, applications
  are expected to call render().

  \li QQuickRenderControl::sceneChanged() Indicates that the scene has changed
  meaning that, before rendering, polishing and synchronizing is also necessary.

  \endlist

  To send events, for example mouse or keyboard events, to the scene, use
  QCoreApplication::sendEvent() with the QQuickWindow instance as the receiver.

  \note In general QQuickRenderControl is supported in combination with all Qt
  Quick backends. However, some functionality, in particular grab(), may not be
  available in all cases.

  \inmodule QtQuick
*/

QSGContext *QQuickRenderControlPrivate::sg = 0;

QQuickRenderControlPrivate::QQuickRenderControlPrivate()
    : initialized(0),
      window(0)
{
    if (!sg) {
        qAddPostRoutine(cleanup);
        sg = QSGContext::createDefaultContext();
    }
    rc = sg->createRenderContext();
}

void QQuickRenderControlPrivate::cleanup()
{
    delete sg;
    sg = 0;
}

/*!
   Constructs a QQuickRenderControl object, with parent
   object \a parent.
*/
QQuickRenderControl::QQuickRenderControl(QObject *parent)
    : QObject(*(new QQuickRenderControlPrivate), parent)
{
}

/*!
  Destroys the instance. Releases all scenegraph resources.

  \sa invalidate()
 */
QQuickRenderControl::~QQuickRenderControl()
{
    Q_D(QQuickRenderControl);

    invalidate();

    if (d->window)
        QQuickWindowPrivate::get(d->window)->renderControl = 0;

    // It is likely that the cleanup in windowDestroyed() is not called since
    // the standard pattern is to destroy the rendercontrol before the QQuickWindow.
    // Do it here.
    d->windowDestroyed();

    delete d->rc;
}

void QQuickRenderControlPrivate::windowDestroyed()
{
    if (window) {
        rc->invalidate();
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);

        delete QQuickWindowPrivate::get(window)->animationController;
        QQuickWindowPrivate::get(window)->animationController = 0;

#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
        QQuickOpenGLShaderEffectMaterial::cleanupMaterialCache();
#endif

        window = 0;
    }
}

/*!
  Prepares rendering the Qt Quick scene outside the gui thread.

  \a targetThread specifies the thread on which synchronization and
  rendering will happen. There is no need to call this function in a
  single threaded scenario.
 */
void QQuickRenderControl::prepareThread(QThread *targetThread)
{
    Q_D(QQuickRenderControl);
    d->rc->moveToThread(targetThread);
    QQuickWindowPrivate::get(d->window)->animationController->moveToThread(targetThread);
}

/*!
  Initializes the scene graph resources. The context \a gl has to be the
  current OpenGL context or null if it is not relevant because a Qt Quick
  backend other than OpenGL is in use.

  \note Qt Quick does not take ownership of the context. It is up to the
  application to destroy it after a call to invalidate() or after the
  QQuickRenderControl instance is destroyed.
 */
void QQuickRenderControl::initialize(QOpenGLContext *gl)
{

    Q_D(QQuickRenderControl);
#ifndef QT_NO_OPENGL
    if (!d->window) {
        qWarning("QQuickRenderControl::initialize called with no associated window");
        return;
    }

    if (QOpenGLContext::currentContext() != gl) {
        qWarning("QQuickRenderControl::initialize called with incorrect current context");
        return;
    }

    // It is the caller's responsiblity to make a context/surface current.
    // It cannot be done here since the surface to use may not be the
    // surface belonging to window. In fact window may not have a native
    // window/surface at all.
    d->rc->initialize(gl);
#else
    Q_UNUSED(gl)
#endif
    d->initialized = true;
}

/*!
  This function should be called as late as possible before
  sync(). In a threaded scenario, rendering can happen in parallel
  with this function.
 */
void QQuickRenderControl::polishItems()
{
    Q_D(QQuickRenderControl);
    if (!d->window)
        return;

    QQuickWindowPrivate *cd = QQuickWindowPrivate::get(d->window);
    cd->flushFrameSynchronousEvents();
    if (!d->window)
        return;
    cd->polishItems();
}

/*!
  This function is used to synchronize the QML scene with the rendering scene
  graph.

  If a dedicated render thread is used, the GUI thread should be blocked for the
  duration of this call.

  \return \e true if the synchronization changed the scene graph.
 */
bool QQuickRenderControl::sync()
{
    Q_D(QQuickRenderControl);
    if (!d->window)
        return false;

    QQuickWindowPrivate *cd = QQuickWindowPrivate::get(d->window);
    cd->syncSceneGraph();

    // TODO: find out if the sync actually caused a scenegraph update.
    return true;
}

/*!
  Stop rendering and release resources. Requires a current context.

  This is the equivalent of the cleanup operations that happen with a
  real QQuickWindow when the window becomes hidden.

  This function is called from the destructor. Therefore there will
  typically be no need to call it directly. Pay attention however to
  the fact that this requires the context, that was passed to
  initialize(), to be the current one at the time of destroying the
  QQuickRenderControl instance.

  Once invalidate() has been called, it is possible to reuse the
  QQuickRenderControl instance by calling initialize() again.

  \note This function does not take
  QQuickWindow::persistentSceneGraph() or
  QQuickWindow::persistentOpenGLContext() into account. This means
  that context-specific resources are always released.
 */
void QQuickRenderControl::invalidate()
{
    Q_D(QQuickRenderControl);
    if (!d->initialized)
        return;

    if (!d->window)
        return;

    QQuickWindowPrivate *cd = QQuickWindowPrivate::get(d->window);
    cd->fireAboutToStop();
    cd->cleanupNodesOnShutdown();

    // We must invalidate since the context can potentially be destroyed by the
    // application right after returning from this function. Invalidating is
    // also essential to allow a subsequent initialize() to succeed.
    d->rc->invalidate();

    d->initialized = false;
}

/*!
  Renders the scenegraph using the current context.
 */
void QQuickRenderControl::render()
{
    Q_D(QQuickRenderControl);
    if (!d->window)
        return;

    QQuickWindowPrivate *cd = QQuickWindowPrivate::get(d->window);
    cd->renderSceneGraph(d->window->size());
}

/*!
    \fn void QQuickRenderControl::renderRequested()

    This signal is emitted when the scene graph needs to be rendered. It is not necessary to call sync().

    \note Avoid triggering rendering directly when this signal is
    emitted. Instead, prefer deferring it by using a timer for example. This
    will lead to better performance.
*/

/*!
    \fn void QQuickRenderControl::sceneChanged()

    This signal is emitted when the scene graph is updated, meaning that
    polishItems() and sync() needs to be called. If sync() returns
    true, then render() needs to be called.

    \note Avoid triggering polishing, synchronization and rendering directly
    when this signal is emitted. Instead, prefer deferring it by using a timer
    for example. This will lead to better performance.
*/

/*!
  Grabs the contents of the scene and returns it as an image.

  \note Requires the context to be current.
 */
QImage QQuickRenderControl::grab()
{
    Q_D(QQuickRenderControl);
    if (!d->window)
        return QImage();

    QImage grabContent;

    if (d->window->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL) {
#ifndef QT_NO_OPENGL
        render();
        grabContent = qt_gl_read_framebuffer(d->window->size() * d->window->effectiveDevicePixelRatio(), false, false);
#endif
    } else if (d->window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        QQuickWindowPrivate *cd = QQuickWindowPrivate::get(d->window);
        QSGSoftwareRenderer *softwareRenderer = static_cast<QSGSoftwareRenderer *>(cd->renderer);
        if (softwareRenderer) {
            const qreal dpr = d->window->effectiveDevicePixelRatio();
            const QSize imageSize = d->window->size() * dpr;
            grabContent = QImage(imageSize, QImage::Format_ARGB32_Premultiplied);
            grabContent.setDevicePixelRatio(dpr);
            QPaintDevice *prevDev = softwareRenderer->currentPaintDevice();
            softwareRenderer->setCurrentPaintDevice(&grabContent);
            softwareRenderer->markDirty();
            render();
            softwareRenderer->setCurrentPaintDevice(prevDev);
        }
    } else {
        qWarning("QQuickRenderControl: grabs are not supported with the current Qt Quick backend");
    }

    return grabContent;
}

void QQuickRenderControlPrivate::update()
{
    Q_Q(QQuickRenderControl);
    emit q->renderRequested();
}

void QQuickRenderControlPrivate::maybeUpdate()
{
    Q_Q(QQuickRenderControl);
    emit q->sceneChanged();
}

/*!
  \fn QWindow *QQuickRenderControl::renderWindow(QPoint *offset)

  Reimplemented in subclasses to return the real window this render control
  is rendering into.

  If \a offset in non-null, it is set to the offset of the control
  inside the window.

  \note While not mandatory, reimplementing this function becomes essential for
  supporting multiple screens with different device pixel ratios and properly positioning
  popup windows opened from QML. Therefore providing it in subclasses is highly
  recommended.
*/

/*!
  Returns the real window that \a win is being rendered to, if any.

  If \a offset in non-null, it is set to the offset of the rendering
  inside its window.

 */
QWindow *QQuickRenderControl::renderWindowFor(QQuickWindow *win, QPoint *offset)
{
    if (!win)
        return 0;
    QQuickRenderControl *rc = QQuickWindowPrivate::get(win)->renderControl;
    if (rc)
        return rc->renderWindow(offset);
    return 0;
}

QT_END_NAMESPACE
