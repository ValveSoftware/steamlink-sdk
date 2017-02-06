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

#include "qquickwidget.h"
#include "qquickwidget_p.h"

#include "private/qquickwindow_p.h"
#include "private/qquickitem_p.h"
#include "private/qquickitemchangelistener_p.h"
#include "private/qquickrendercontrol_p.h"

#include "private/qsgsoftwarerenderer_p.h"

#include <private/qqmldebugconnector_p.h>
#include <private/qquickprofiler_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qqmlmemoryprofiler_p.h>

#include <QtQml/qqmlengine.h>
#include <private/qqmlengine_p.h>
#include <QtCore/qbasictimer.h>
#include <QtGui/QOffscreenSurface>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformintegration.h>

#ifndef QT_NO_OPENGL
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/private/qopenglextensions_p.h>
#endif
#include <QtGui/QPainter>

#include <QtQuick/QSGRendererInterface>

#ifdef Q_OS_WIN
#  include <QtWidgets/QMessageBox>
#  include <QtCore/QLibraryInfo>
#  include <QtCore/qt_windows.h>
#endif

QT_BEGIN_NAMESPACE

#ifndef QT_NO_OPENGL
extern Q_GUI_EXPORT QImage qt_gl_read_framebuffer(const QSize &size, bool alpha_format, bool include_alpha);
#endif

class QQuickWidgetRenderControl : public QQuickRenderControl
{
public:
    QQuickWidgetRenderControl(QQuickWidget *quickwidget) : m_quickWidget(quickwidget) {}
    QWindow *renderWindow(QPoint *offset) Q_DECL_OVERRIDE {
        if (offset)
            *offset = m_quickWidget->mapTo(m_quickWidget->window(), QPoint());
        return m_quickWidget->window()->windowHandle();
    }
private:
    QQuickWidget *m_quickWidget;
};

void QQuickWidgetPrivate::init(QQmlEngine* e)
{
    Q_Q(QQuickWidget);

    renderControl = new QQuickWidgetRenderControl(q);
    offscreenWindow = new QQuickWindow(renderControl);
    offscreenWindow->setTitle(QString::fromLatin1("Offscreen"));
    // Do not call create() on offscreenWindow.

    // Check if the Software Adaptation is being used
    auto sgRendererInterface = offscreenWindow->rendererInterface();
    if (sgRendererInterface && sgRendererInterface->graphicsApi() == QSGRendererInterface::Software)
        useSoftwareRenderer = true;

    if (!useSoftwareRenderer) {
#ifndef QT_NO_OPENGL
        if (QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::RasterGLSurface))
            setRenderToTexture();
        else
#endif
            qWarning("QQuickWidget is not supported on this platform.");
    }

    engine = e;

    if (!engine.isNull() && !engine.data()->incubationController())
        engine.data()->setIncubationController(offscreenWindow->incubationController());

#ifndef QT_NO_DRAGANDDROP
    q->setAcceptDrops(true);
#endif

    QWidget::connect(offscreenWindow, SIGNAL(sceneGraphInitialized()), q, SLOT(createFramebufferObject()));
    QWidget::connect(offscreenWindow, SIGNAL(sceneGraphInvalidated()), q, SLOT(destroyFramebufferObject()));
    QObject::connect(renderControl, SIGNAL(renderRequested()), q, SLOT(triggerUpdate()));
    QObject::connect(renderControl, SIGNAL(sceneChanged()), q, SLOT(triggerUpdate()));
}

void QQuickWidgetPrivate::ensureEngine() const
{
    Q_Q(const QQuickWidget);
    if (!engine.isNull())
        return;

    engine = new QQmlEngine(const_cast<QQuickWidget*>(q));
    engine.data()->setIncubationController(offscreenWindow->incubationController());
}

void QQuickWidgetPrivate::invalidateRenderControl()
{
#ifndef QT_NO_OPENGL
    if (!useSoftwareRenderer) {
        if (!context) // this is not an error, could be called before creating the context, or multiple times
            return;

        bool success = context->makeCurrent(offscreenSurface);
        if (!success) {
            qWarning("QQuickWidget::invalidateRenderControl could not make context current");
            return;
        }
    }
#endif

    renderControl->invalidate();
}

void QQuickWidgetPrivate::handleWindowChange()
{
    if (offscreenWindow->isPersistentSceneGraph() && qGuiApp->testAttribute(Qt::AA_ShareOpenGLContexts))
        return;

    // In case of !isPersistentSceneGraph or when we need a new context due to
    // the need to share resources with the new window's context, we must both
    // invalidate the scenegraph and destroy the context. With
    // QQuickRenderControl destroying the context must be preceded by an
    // invalidate to prevent being left with dangling context references in the
    // rendercontrol.

    invalidateRenderControl();

    if (!useSoftwareRenderer)
        destroyContext();
}

QQuickWidgetPrivate::QQuickWidgetPrivate()
    : root(0)
    , component(0)
    , offscreenWindow(0)
    , offscreenSurface(0)
    , renderControl(0)
#ifndef QT_NO_OPENGL
    , fbo(0)
    , resolvedFbo(0)
    , context(0)
#endif
    , resizeMode(QQuickWidget::SizeViewToRootObject)
    , initialSize(0,0)
    , eventPending(false)
    , updatePending(false)
    , fakeHidden(false)
    , requestedSamples(0)
    , useSoftwareRenderer(false)
{
}

QQuickWidgetPrivate::~QQuickWidgetPrivate()
{
    invalidateRenderControl();

    if (useSoftwareRenderer) {
        delete renderControl;
        delete offscreenWindow;
    } else {
#ifndef QT_NO_OPENGL
        // context and offscreenSurface are current at this stage, if the context was created.
        Q_ASSERT(!context || (QOpenGLContext::currentContext() == context && context->surface() == offscreenSurface));
        delete renderControl; // always delete the rendercontrol first
        delete offscreenWindow;
        delete resolvedFbo;
        delete fbo;

        destroyContext();
#endif
    }
}

void QQuickWidgetPrivate::execute()
{
    Q_Q(QQuickWidget);
    ensureEngine();

    if (root) {
        delete root;
        root = 0;
    }
    if (component) {
        delete component;
        component = 0;
    }
    if (!source.isEmpty()) {
        QML_MEMORY_SCOPE_URL(engine.data()->baseUrl().resolved(source));
        component = new QQmlComponent(engine.data(), source, q);
        if (!component->isLoading()) {
            q->continueExecute();
        } else {
            QObject::connect(component, SIGNAL(statusChanged(QQmlComponent::Status)),
                             q, SLOT(continueExecute()));
        }
    }
}

void QQuickWidgetPrivate::itemGeometryChanged(QQuickItem *resizeItem, QQuickGeometryChange change,
                                              const QRectF &oldGeometry)
{
    Q_Q(QQuickWidget);
    if (resizeItem == root && resizeMode == QQuickWidget::SizeViewToRootObject) {
        // wait for both width and height to be changed
        resizetimer.start(0,q);
    }
    QQuickItemChangeListener::itemGeometryChanged(resizeItem, change, oldGeometry);
}

void QQuickWidgetPrivate::render(bool needsSync)
{
    if (!useSoftwareRenderer) {
#ifndef QT_NO_OPENGL
        // createFramebufferObject() bails out when the size is empty. In this case
        // we cannot render either.
        if (!fbo)
            return;

        Q_ASSERT(context);

        if (!context->makeCurrent(offscreenSurface)) {
            qWarning("QQuickWidget: Cannot render due to failing makeCurrent()");
            return;
        }

        QOpenGLContextPrivate::get(context)->defaultFboRedirect = fbo->handle();

        if (needsSync) {
            renderControl->polishItems();
            renderControl->sync();
        }

        renderControl->render();

        if (resolvedFbo) {
            QRect rect(QPoint(0, 0), fbo->size());
            QOpenGLFramebufferObject::blitFramebuffer(resolvedFbo, rect, fbo, rect);
        }

        static_cast<QOpenGLExtensions *>(context->functions())->flushShared();

        QOpenGLContextPrivate::get(context)->defaultFboRedirect = 0;
#endif
    } else {
        //Software Renderer
        if (needsSync) {
            renderControl->polishItems();
            renderControl->sync();
        }

        QQuickWindowPrivate *cd = QQuickWindowPrivate::get(offscreenWindow);
        auto softwareRenderer = static_cast<QSGSoftwareRenderer*>(cd->renderer);
        if (softwareRenderer && !softwareImage.isNull()) {
            softwareRenderer->setCurrentPaintDevice(&softwareImage);
            renderControl->render();

            updateRegion += softwareRenderer->flushRegion();
        }
    }
}

void QQuickWidgetPrivate::renderSceneGraph()
{
    Q_Q(QQuickWidget);
    updatePending = false;

    if (!q->isVisible() || fakeHidden)
        return;

    if (!useSoftwareRenderer) {
        QOpenGLContext *context = offscreenWindow->openglContext();
        if (!context) {
            qWarning("QQuickWidget: Attempted to render scene with no context");
            return;
        }

        Q_ASSERT(offscreenSurface);
    }

    render(true);

#ifndef QT_NO_GRAPHICSVIEW
    if (q->window()->graphicsProxyWidget())
        QWidgetPrivate::nearestGraphicsProxyWidget(q)->update();
    else
#endif
    {
        if (!useSoftwareRenderer)
            q->update(); // schedule composition
        else if (!updateRegion.isEmpty())
            q->update(updateRegion);
    }
}

QImage QQuickWidgetPrivate::grabFramebuffer()
{
    if (!useSoftwareRenderer) {
#ifndef QT_NO_OPENGL
        if (!context)
            return QImage();

        context->makeCurrent(offscreenSurface);
#endif
    }
    return renderControl->grab();
}

QObject *QQuickWidgetPrivate::focusObject()
{
    return offscreenWindow ? offscreenWindow->focusObject() : 0;
}

/*!
    \module QtQuickWidgets
    \title Qt Quick Widgets C++ Classes
    \ingroup modules
    \brief The C++ API provided by the Qt Quick Widgets module
    \qtvariable quickwidgets

    To link against the module, add this line to your \l qmake
    \c .pro file:

    \code
    QT += quickwidgets
    \endcode

    For more information, see the QQuickWidget class documentation.
*/

/*!
    \class QQuickWidget
    \since 5.3
    \brief The QQuickWidget class provides a widget for displaying a Qt Quick user interface.

    \inmodule QtQuickWidgets

    This is a convenience wrapper for QQuickWindow which will automatically load and display a QML
    scene when given the URL of the main source file. Alternatively, you can instantiate your own
    objects using QQmlComponent and place them in a manually set up QQuickWidget.

    Typical usage:

    \code
    QQuickWidget *view = new QQuickWidget;
    view->setSource(QUrl::fromLocalFile("myqmlfile.qml"));
    view->show();
    \endcode

    To receive errors related to loading and executing QML with QQuickWidget,
    you can connect to the statusChanged() signal and monitor for QQuickWidget::Error.
    The errors are available via QQuickWidget::errors().

    QQuickWidget also manages sizing of the view and root object. By default, the \l resizeMode
    is SizeViewToRootObject, which will load the component and resize it to the
    size of the view. Alternatively the resizeMode may be set to SizeRootObjectToView which
    will resize the view to the size of the root object.

    \note QQuickWidget is an alternative to using QQuickView and QWidget::createWindowContainer().
    The restrictions on stacking order do not apply, making QQuickWidget the more flexible
    alternative, behaving more like an ordinary widget. This comes at the expense of
    performance. Unlike QQuickWindow and QQuickView, QQuickWidget involves rendering into OpenGL
    framebuffer objects. This will naturally carry a minor performance hit.

    \note Using QQuickWidget disables the threaded render loop on all platforms. This means that
    some of the benefits of threaded rendering, for example \l Animator classes and vsync driven
    animations, will not be available.

    \note Avoid calling winId() on a QQuickWidget. This function triggers the creation of
    a native window, resulting in reduced performance and possibly rendering glitches. The
    entire purpose of QQuickWidget is to render Quick scenes without a separate native
    window, hence making it a native widget should always be avoided.

    \section1 Scene graph and context persistency

    QQuickWidget honors QQuickWindow::isPersistentSceneGraph(), meaning that
    applications can decide - by calling
    QQuickWindow::setPersistentSceneGraph() on the window returned from the
    quickWindow() function - to let scenegraph nodes and other Qt Quick scene
    related resources be released whenever the widget becomes hidden. By default
    persistency is enabled, just like with QQuickWindow.

    When running with the OpenGL backend of the scene graph, QQuickWindow
    offers the possibility to disable persistent OpenGL contexts as well. This
    setting is currently ignored by QQuickWidget and the context is always
    persistent. The OpenGL context is thus not destroyed when hiding the
    widget. The context is destroyed only when the widget is destroyed or when
    the widget gets reparented into another top-level widget's child hierarchy.
    However, some applications, in particular those that have their own
    graphics resources due to performing custom OpenGL rendering in the Qt
    Quick scene, may wish to disable the latter since they may not be prepared
    to handle the loss of the context when moving a QQuickWidget into another
    window. Such applications can set the
    QCoreApplication::AA_ShareOpenGLContexts attribute. For a discussion on the
    details of resource initialization and cleanup, refer to the QOpenGLWidget
    documentation.

    \note QQuickWidget offers less fine-grained control over its internal
    OpenGL context than QOpenGLWidget, and there are subtle differences, most
    notably that disabling the persistent scene graph will lead to destroying
    the context on a window change regardless of the presence of
    QCoreApplication::AA_ShareOpenGLContexts.

    \section1 Limitations

    Putting other widgets underneath and making the QQuickWidget transparent will not lead
    to the expected results: the widgets underneath will not be visible. This is because
    in practice the QQuickWidget is drawn before all other regular, non-OpenGL widgets,
    and so see-through types of solutions are not feasible. Other type of layouts, like
    having widgets on top of the QQuickWidget, will function as expected.

    When absolutely necessary, this limitation can be overcome by setting the
    Qt::WA_AlwaysStackOnTop attribute on the QQuickWidget. Be aware, however that this
    breaks stacking order. For example it will not be possible to have other widgets on
    top of the QQuickWidget, so it should only be used in situations where a
    semi-transparent QQuickWidget with other widgets visible underneath is required.

    This limitation only applies when there are other widgets underneath the QQuickWidget
    inside the same window. Making the window semi-transparent, with other applications
    and the desktop visible in the background, is done in the traditional way: Set
    Qt::WA_TranslucentBackground on the top-level window, request an alpha channel, and
    change the Qt Quick Scenegraph's clear color to Qt::transparent via setClearColor().

    \section1 Support when not using OpenGL

    In addition to OpenGL, the \c software backend of Qt Quick also supports
    QQuickWidget. Other backends, for example the Direct 3D 12 one, are not
    compatible however and attempting to construct a QQuickWidget will lead to
    problems.

    \sa {Exposing Attributes of C++ Types to QML}, {Qt Quick Widgets Example}, QQuickView
*/


/*! \fn void QQuickWidget::statusChanged(QQuickWidget::Status status)
    This signal is emitted when the component's current \a status changes.
*/

/*!
  Constructs a QQuickWidget with the given \a parent.
  The default value of \a parent is 0.

*/
QQuickWidget::QQuickWidget(QWidget *parent)
: QWidget(*(new QQuickWidgetPrivate), parent, 0)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    d_func()->init();
}

/*!
  Constructs a QQuickWidget with the given QML \a source and \a parent.
  The default value of \a parent is 0.

*/
QQuickWidget::QQuickWidget(const QUrl &source, QWidget *parent)
    : QQuickWidget(parent)
{
    setSource(source);
}

/*!
  Constructs a QQuickWidget with the given QML \a engine and \a parent.

  Note: In this case, the QQuickWidget does not own the given \a engine object;
  it is the caller's responsibility to destroy the engine. If the \a engine is deleted
  before the view, status() will return QQuickWidget::Error.

  \sa Status, status(), errors()
*/
QQuickWidget::QQuickWidget(QQmlEngine* engine, QWidget *parent)
    : QWidget(*(new QQuickWidgetPrivate), parent, 0)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    d_func()->init(engine);
}

/*!
  Destroys the QQuickWidget.
*/
QQuickWidget::~QQuickWidget()
{
    // Ensure that the component is destroyed before the engine; the engine may
    // be a child of the QQuickWidgetPrivate, and will be destroyed by its dtor
    Q_D(QQuickWidget);
    delete d->root;
    d->root = 0;
}

/*!
  \property QQuickWidget::source
  \brief The URL of the source of the QML component.

  Ensure that the URL provided is full and correct, in particular, use
  \l QUrl::fromLocalFile() when loading a file from the local filesystem.

  \note Setting a source URL will result in the QML component being
  instantiated, even if the URL is unchanged from the current value.
*/

/*!
    Sets the source to the \a url, loads the QML component and instantiates it.

    Ensure that the URL provided is full and correct, in particular, use
    \l QUrl::fromLocalFile() when loading a file from the local filesystem.

    Calling this method multiple times with the same URL will result
    in the QML component being reinstantiated.
 */
void QQuickWidget::setSource(const QUrl& url)
{
    Q_D(QQuickWidget);
    d->source = url;
    d->execute();
}

/*!
    \internal

    Sets the source \a url, \a component and content \a item (root of the QML object hierarchy) directly.
 */
void QQuickWidget::setContent(const QUrl& url, QQmlComponent *component, QObject* item)
{
    Q_D(QQuickWidget);
    d->source = url;
    d->component = component;

    if (d->component && d->component->isError()) {
        const QList<QQmlError> errorList = d->component->errors();
        for (const QQmlError &error : errorList) {
            QMessageLogger(error.url().toString().toLatin1().constData(), error.line(), 0).warning()
                    << error;
        }
        emit statusChanged(status());
        return;
    }

    d->setRootObject(item);
    emit statusChanged(status());
}

/*!
  Returns the source URL, if set.

  \sa setSource()
 */
QUrl QQuickWidget::source() const
{
    Q_D(const QQuickWidget);
    return d->source;
}

/*!
  Returns a pointer to the QQmlEngine used for instantiating
  QML Components.
 */
QQmlEngine* QQuickWidget::engine() const
{
    Q_D(const QQuickWidget);
    d->ensureEngine();
    return const_cast<QQmlEngine *>(d->engine.data());
}

/*!
  This function returns the root of the context hierarchy.  Each QML
  component is instantiated in a QQmlContext.  QQmlContext's are
  essential for passing data to QML components.  In QML, contexts are
  arranged hierarchically and this hierarchy is managed by the
  QQmlEngine.
 */
QQmlContext* QQuickWidget::rootContext() const
{
    Q_D(const QQuickWidget);
    d->ensureEngine();
    return d->engine.data()->rootContext();
}

/*!
    \enum QQuickWidget::Status
    Specifies the loading status of the QQuickWidget.

    \value Null This QQuickWidget has no source set.
    \value Ready This QQuickWidget has loaded and created the QML component.
    \value Loading This QQuickWidget is loading network data.
    \value Error One or more errors occurred. Call errors() to retrieve a list
           of errors.
*/

/*! \enum QQuickWidget::ResizeMode

  This enum specifies how to resize the view.

  \value SizeViewToRootObject The view resizes with the root item in the QML.
  \value SizeRootObjectToView The view will automatically resize the root item to the size of the view.
*/

/*!
    \fn void QQuickWidget::sceneGraphError(QQuickWindow::SceneGraphError error, const QString &message)

    This signal is emitted when an \a error occurred during scene graph initialization.

    Applications should connect to this signal if they wish to handle errors,
    like OpenGL context creation failures, in a custom way. When no slot is
    connected to the signal, the behavior will be different: Quick will print
    the \a message, or show a message box, and terminate the application.

    This signal will be emitted from the gui thread.

    \sa QQuickWindow::sceneGraphError()
 */

/*!
    \property QQuickWidget::status
    The component's current \l{QQuickWidget::Status} {status}.
*/

QQuickWidget::Status QQuickWidget::status() const
{
    Q_D(const QQuickWidget);
    if (!d->engine && !d->source.isEmpty())
        return QQuickWidget::Error;

    if (!d->component)
        return QQuickWidget::Null;

    if (d->component->status() == QQmlComponent::Ready && !d->root)
        return QQuickWidget::Error;

    return QQuickWidget::Status(d->component->status());
}

/*!
    Return the list of errors that occurred during the last compile or create
    operation. When the status is not \l Error, an empty list is returned.

    \sa status
*/
QList<QQmlError> QQuickWidget::errors() const
{
    Q_D(const QQuickWidget);
    QList<QQmlError> errs;

    if (d->component)
        errs = d->component->errors();

    if (!d->engine && !d->source.isEmpty()) {
        QQmlError error;
        error.setDescription(QLatin1String("QQuickWidget: invalid qml engine."));
        errs << error;
    }
    if (d->component && d->component->status() == QQmlComponent::Ready && !d->root) {
        QQmlError error;
        error.setDescription(QLatin1String("QQuickWidget: invalid root object."));
        errs << error;
    }

    return errs;
}

/*!
    \property QQuickWidget::resizeMode
    \brief Determines whether the view should resize the window contents.

    If this property is set to SizeViewToRootObject (the default), the view
    resizes to the size of the root item in the QML.

    If this property is set to SizeRootObjectToView, the view will
    automatically resize the root item to the size of the view.

    Regardless of this property, the sizeHint of the view
    is the initial size of the root item. Note though that
    since QML may load dynamically, that size may change.

    \sa initialSize()
*/

void QQuickWidget::setResizeMode(ResizeMode mode)
{
    Q_D(QQuickWidget);
    if (d->resizeMode == mode)
        return;

    if (d->root) {
        if (d->resizeMode == SizeViewToRootObject) {
            QQuickItemPrivate *p = QQuickItemPrivate::get(d->root);
            p->removeItemChangeListener(d, QQuickItemPrivate::Geometry);
        }
    }

    d->resizeMode = mode;
    if (d->root) {
        d->initResize();
    }
}

void QQuickWidgetPrivate::initResize()
{
    if (root) {
        if (resizeMode == QQuickWidget::SizeViewToRootObject) {
            QQuickItemPrivate *p = QQuickItemPrivate::get(root);
            p->addItemChangeListener(this, QQuickItemPrivate::Geometry);
        }
    }
    updateSize();
}

void QQuickWidgetPrivate::updateSize()
{
    Q_Q(QQuickWidget);
    if (!root)
        return;

    if (resizeMode == QQuickWidget::SizeViewToRootObject) {
        QSize newSize = QSize(root->width(), root->height());
        if (newSize.isValid() && newSize != q->size()) {
            q->resize(newSize);
        }
    } else if (resizeMode == QQuickWidget::SizeRootObjectToView) {
        bool needToUpdateWidth = !qFuzzyCompare(q->width(), root->width());
        bool needToUpdateHeight = !qFuzzyCompare(q->height(), root->height());

        if (needToUpdateWidth && needToUpdateHeight)
            root->setSize(QSizeF(q->width(), q->height()));
        else if (needToUpdateWidth)
            root->setWidth(q->width());
        else if (needToUpdateHeight)
            root->setHeight(q->height());
    }
}

/*!
  \internal

  Update the position of the offscreen window, so it matches the position of the QQuickWidget.
 */
void QQuickWidgetPrivate::updatePosition()
{
    Q_Q(QQuickWidget);
    if (offscreenWindow == 0)
        return;

    const QPoint &pos = q->mapToGlobal(QPoint(0, 0));
    if (offscreenWindow->position() != pos)
        offscreenWindow->setPosition(pos);
}

QSize QQuickWidgetPrivate::rootObjectSize() const
{
    QSize rootObjectSize(0,0);
    int widthCandidate = -1;
    int heightCandidate = -1;
    if (root) {
        widthCandidate = root->width();
        heightCandidate = root->height();
    }
    if (widthCandidate > 0) {
        rootObjectSize.setWidth(widthCandidate);
    }
    if (heightCandidate > 0) {
        rootObjectSize.setHeight(heightCandidate);
    }
    return rootObjectSize;
}

void QQuickWidgetPrivate::handleContextCreationFailure(const QSurfaceFormat &format, bool isEs)
{
    Q_Q(QQuickWidget);

    QString translatedMessage;
    QString untranslatedMessage;
    QQuickWindowPrivate::contextCreationFailureMessage(format, &translatedMessage, &untranslatedMessage, isEs);

    static const QMetaMethod errorSignal = QMetaMethod::fromSignal(&QQuickWidget::sceneGraphError);
    const bool signalConnected = q->isSignalConnected(errorSignal);
    if (signalConnected)
        emit q->sceneGraphError(QQuickWindow::ContextNotAvailable, translatedMessage);

#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
    if (!signalConnected && !QLibraryInfo::isDebugBuild() && !GetConsoleWindow())
        QMessageBox::critical(q, QCoreApplication::applicationName(), translatedMessage);
#endif // Q_OS_WIN && !Q_OS_WINRT
    if (!signalConnected)
        qFatal("%s", qPrintable(untranslatedMessage));
}

// Never called by Software Rendering backend
void QQuickWidgetPrivate::createContext()
{
#ifndef QT_NO_OPENGL
    Q_Q(QQuickWidget);

    // On hide-show we may invalidate() (when !isPersistentSceneGraph) but our
    // context is kept. We may need to initialize() again, though.
    const bool reinit = context && !offscreenWindow->openglContext();

    if (!reinit) {
        if (context)
            return;

        context = new QOpenGLContext;
        context->setFormat(offscreenWindow->requestedFormat());

        QOpenGLContext *shareContext = qt_gl_global_share_context();
        if (!shareContext)
            shareContext = QWidgetPrivate::get(q->window())->shareContext();
        if (shareContext) {
            context->setShareContext(shareContext);
            context->setScreen(shareContext->screen());
        }
        if (!context->create()) {
            const bool isEs = context->isOpenGLES();
            delete context;
            context = 0;
            handleContextCreationFailure(offscreenWindow->requestedFormat(), isEs);
            return;
        }

        offscreenSurface = new QOffscreenSurface;
        // Pass the context's format(), which, now that the underlying platform context is created,
        // contains a QSurfaceFormat representing the _actual_ format of the underlying
        // configuration. This is essential to get a surface that is compatible with the context.
        offscreenSurface->setFormat(context->format());
        offscreenSurface->setScreen(context->screen());
        offscreenSurface->create();
    }

    if (context->makeCurrent(offscreenSurface)) {
        if (!offscreenWindow->openglContext())
            renderControl->initialize(context);
    } else
#endif
        qWarning("QQuickWidget: Failed to make context current");
}

// Never called by Software Rendering backend
void QQuickWidgetPrivate::destroyContext()
{
    delete offscreenSurface;
    offscreenSurface = 0;
#ifndef QT_NO_OPENGL
    delete context;
    context = 0;
#endif
}

void QQuickWidget::createFramebufferObject()
{
    Q_D(QQuickWidget);

    // Could come from Show -> createContext -> sceneGraphInitialized in which case the size may
    // still be invalid on some platforms. Bail out. A resize will come later on.
    if (size().isEmpty())
        return;

    // Even though this is just an offscreen window we should set the position on it, as it might be
    // useful for an item to know the actual position of the scene.
    // Note: The position will be update when we get a move event (see: updatePosition()).
    const QPoint &globalPos = mapToGlobal(QPoint(0, 0));
    d->offscreenWindow->setGeometry(globalPos.x(), globalPos.y(), width(), height());

    if (d->useSoftwareRenderer) {
        const QSize imageSize = size() * devicePixelRatio();
        d->softwareImage = QImage(imageSize, QImage::Format_ARGB32_Premultiplied);
        d->softwareImage.setDevicePixelRatio(devicePixelRatio());
        return;
    }

#ifndef QT_NO_OPENGL
    QOpenGLContext *context = d->offscreenWindow->openglContext();

    if (!context) {
        qWarning("QQuickWidget: Attempted to create FBO with no context");
        return;
    }

    QOpenGLContext *shareWindowContext = QWidgetPrivate::get(window())->shareContext();
    if (shareWindowContext && context->shareContext() != shareWindowContext) {
        context->setShareContext(shareWindowContext);
        context->setScreen(shareWindowContext->screen());
        if (!context->create())
            qWarning("QQuickWidget: Failed to recreate context");
        // The screen may be different so we must recreate the offscreen surface too.
        // Unlike QOpenGLContext, QOffscreenSurface's create() does not recreate so have to destroy() first.
        d->offscreenSurface->destroy();
        d->offscreenSurface->setScreen(context->screen());
        d->offscreenSurface->create();
    }

    context->makeCurrent(d->offscreenSurface);

    int samples = d->requestedSamples;
    if (!QOpenGLExtensions(context).hasOpenGLExtension(QOpenGLExtensions::FramebufferMultisample))
        samples = 0;

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(samples);

    // The default framebuffer for normal windows have sRGB support on OS X which leads to the Qt Quick text item
    // utilizing sRGB blending. To get identical results with QQuickWidget we have to have our framebuffer backed
    // by an sRGB texture.
#ifdef Q_OS_OSX
    if (context->hasExtension("GL_ARB_framebuffer_sRGB")
            && context->hasExtension("GL_EXT_texture_sRGB")
            && context->hasExtension("GL_EXT_texture_sRGB_decode"))
        format.setInternalTextureFormat(GL_SRGB8_ALPHA8_EXT);
#endif

    const QSize fboSize = size() * devicePixelRatio();

    // Could be a simple hide - show, in which case the previous fbo is just fine.
    if (!d->fbo || d->fbo->size() != fboSize) {
        delete d->fbo;
        d->fbo = new QOpenGLFramebufferObject(fboSize, format);
    }

    // When compositing in the backingstore, sampling the sRGB texture would perform an
    // sRGB-linear conversion which is not what we want when the final framebuffer (the window's)
    // is sRGB too. Disable the conversion.
#ifdef Q_OS_OSX
    if (format.internalTextureFormat() == GL_SRGB8_ALPHA8_EXT) {
        QOpenGLFunctions *funcs = context->functions();
        funcs->glBindTexture(GL_TEXTURE_2D, d->fbo->texture());
        funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT);
    }
#endif

    d->offscreenWindow->setRenderTarget(d->fbo);

    if (samples > 0)
        d->resolvedFbo = new QOpenGLFramebufferObject(fboSize);

    // Sanity check: The window must not have an underlying platform window.
    // Having one would mean create() was called and platforms that only support
    // a single native window were in trouble.
    Q_ASSERT(!d->offscreenWindow->handle());
#endif
}

void QQuickWidget::destroyFramebufferObject()
{
    Q_D(QQuickWidget);

    if (d->useSoftwareRenderer) {
        d->softwareImage = QImage();
        return;
    }

#ifndef QT_NO_OPENGL
    delete d->fbo;
    d->fbo = 0;
    delete d->resolvedFbo;
    d->resolvedFbo = 0;
#endif
}

QQuickWidget::ResizeMode QQuickWidget::resizeMode() const
{
    Q_D(const QQuickWidget);
    return d->resizeMode;
}

/*!
  \internal
 */
void QQuickWidget::continueExecute()
{
    Q_D(QQuickWidget);
    disconnect(d->component, SIGNAL(statusChanged(QQmlComponent::Status)), this, SLOT(continueExecute()));

    if (d->component->isError()) {
        const QList<QQmlError> errorList = d->component->errors();
        for (const QQmlError &error : errorList) {
            QMessageLogger(error.url().toString().toLatin1().constData(), error.line(), 0).warning()
                    << error;
        }
        emit statusChanged(status());
        return;
    }

    QObject *obj = d->component->create();

    if (d->component->isError()) {
        const QList<QQmlError> errorList = d->component->errors();
        for (const QQmlError &error : errorList) {
            QMessageLogger(error.url().toString().toLatin1().constData(), error.line(), 0).warning()
                    << error;
        }
        emit statusChanged(status());
        return;
    }

    d->setRootObject(obj);
    emit statusChanged(status());
}


/*!
  \internal
*/
void QQuickWidgetPrivate::setRootObject(QObject *obj)
{
    Q_Q(QQuickWidget);
    if (root == obj)
        return;
    if (QQuickItem *sgItem = qobject_cast<QQuickItem *>(obj)) {
        root = sgItem;
        sgItem->setParentItem(offscreenWindow->contentItem());
    } else if (qobject_cast<QWindow *>(obj)) {
        qWarning() << "QQuickWidget does not support using windows as a root item." << endl
                   << endl
                   << "If you wish to create your root window from QML, consider using QQmlApplicationEngine instead." << endl;
    } else {
        qWarning() << "QQuickWidget only supports loading of root objects that derive from QQuickItem." << endl
                   << endl
                   << "Ensure your QML code is written for QtQuick 2, and uses a root that is or" << endl
                   << "inherits from QtQuick's Item (not a Timer, QtObject, etc)." << endl;
        delete obj;
        root = 0;
    }
    if (root) {
        initialSize = rootObjectSize();
        bool resized = q->testAttribute(Qt::WA_Resized);
        if ((resizeMode == QQuickWidget::SizeViewToRootObject || !resized) &&
            initialSize != q->size()) {
            q->resize(initialSize);
        }
        initResize();
    }
}

#ifndef QT_NO_OPENGL
GLuint QQuickWidgetPrivate::textureId() const
{
    Q_Q(const QQuickWidget);
    if (!q->isWindow() && q->internalWinId()) {
        qWarning() << "QQuickWidget cannot be used as a native child widget."
                   << "Consider setting Qt::AA_DontCreateNativeWidgetSiblings";
        return 0;
    }
    return resolvedFbo ? resolvedFbo->texture()
        : (fbo ? fbo->texture() : 0);
}
#endif

/*!
  \internal
  Handle item resize and scene updates.
 */
void QQuickWidget::timerEvent(QTimerEvent* e)
{
    Q_D(QQuickWidget);
    if (!e || e->timerId() == d->resizetimer.timerId()) {
        d->updateSize();
        d->resizetimer.stop();
    } else if (e->timerId() == d->updateTimer.timerId()) {
        d->eventPending = false;
        d->updateTimer.stop();
        if (d->updatePending)
            d->renderSceneGraph();
    }
}

/*!
    \internal
    Preferred size follows the root object geometry.
*/
QSize QQuickWidget::sizeHint() const
{
    Q_D(const QQuickWidget);
    QSize rootObjectSize = d->rootObjectSize();
    if (rootObjectSize.isEmpty()) {
        return size();
    } else {
        return rootObjectSize;
    }
}

/*!
  Returns the initial size of the root object.

  If \l resizeMode is SizeRootObjectToView, the root object will be
  resized to the size of the view. This function returns the size of the
  root object before it was resized.
*/
QSize QQuickWidget::initialSize() const
{
    Q_D(const QQuickWidget);
    return d->initialSize;
}

/*!
  Returns the view's root \l {QQuickItem} {item}. Can be null
  when setContents/setSource has not been called, if they were called with
  broken QtQuick code or while the QtQuick contents are being created.
 */
QQuickItem *QQuickWidget::rootObject() const
{
    Q_D(const QQuickWidget);
    return d->root;
}

/*!
  \internal
  This function handles the \l {QResizeEvent} {resize event}
  \a e.
 */
void QQuickWidget::resizeEvent(QResizeEvent *e)
{
    Q_D(QQuickWidget);
    if (d->resizeMode == SizeRootObjectToView)
        d->updateSize();

    if (e->size().isEmpty()) {
        //stop rendering
        d->fakeHidden = true;
        return;
    }

    bool needsSync = false;
    if (d->fakeHidden) {
        //restart rendering
        d->fakeHidden = false;
        needsSync = true;
    }

    // Software Renderer
    if (d->useSoftwareRenderer) {
        needsSync = true;
        if (d->softwareImage.size() != size() * devicePixelRatio()) {
            createFramebufferObject();
        }
    } else {
#ifndef QT_NO_OPENGL
        if (d->context) {
            // Bail out when receiving a resize after scenegraph invalidation. This can happen
            // during hide - resize - show sequences and also during application exit.
            if (!d->fbo && !d->offscreenWindow->openglContext())
                return;
            if (!d->fbo || d->fbo->size() != size() * devicePixelRatio()) {
                needsSync = true;
                createFramebufferObject();
            }
        } else {
            // This will result in a scenegraphInitialized() signal which
            // is connected to createFramebufferObject().
            needsSync = true;
            d->createContext();
        }

        QOpenGLContext *context = d->offscreenWindow->openglContext();
        if (!context) {
            qWarning("QQuickWidget::resizeEvent() no OpenGL context");
            return;
        }
#endif
    }

    d->render(needsSync);
}

/*! \reimp */
void QQuickWidget::keyPressEvent(QKeyEvent *e)
{
    Q_D(QQuickWidget);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Key, QQuickProfiler::InputKeyPress, e->key(),
                          e->modifiers());

    QCoreApplication::sendEvent(d->offscreenWindow, e);
}

/*! \reimp */
void QQuickWidget::keyReleaseEvent(QKeyEvent *e)
{
    Q_D(QQuickWidget);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Key, QQuickProfiler::InputKeyRelease, e->key(),
                          e->modifiers());

    QCoreApplication::sendEvent(d->offscreenWindow, e);
}

/*! \reimp */
void QQuickWidget::mouseMoveEvent(QMouseEvent *e)
{
    Q_D(QQuickWidget);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMouseMove, e->localPos().x(),
                          e->localPos().y());

    // Use the constructor taking localPos and screenPos. That puts localPos into the
    // event's localPos and windowPos, and screenPos into the event's screenPos. This way
    // the windowPos in e is ignored and is replaced by localPos. This is necessary
    // because QQuickWindow thinks of itself as a top-level window always.
    QMouseEvent mappedEvent(e->type(), e->localPos(), e->screenPos(), e->button(), e->buttons(), e->modifiers());
    QCoreApplication::sendEvent(d->offscreenWindow, &mappedEvent);
    e->setAccepted(mappedEvent.isAccepted());
}

/*! \reimp */
void QQuickWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    Q_D(QQuickWidget);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMouseDoubleClick,
                          e->button(), e->buttons());

    // As the second mouse press is suppressed in widget windows we emulate it here for QML.
    // See QTBUG-25831
    QMouseEvent pressEvent(QEvent::MouseButtonPress, e->localPos(), e->screenPos(), e->button(),
                           e->buttons(), e->modifiers());
    QCoreApplication::sendEvent(d->offscreenWindow, &pressEvent);
    e->setAccepted(pressEvent.isAccepted());
    QMouseEvent mappedEvent(e->type(), e->localPos(), e->screenPos(), e->button(), e->buttons(),
                            e->modifiers());
    QCoreApplication::sendEvent(d->offscreenWindow, &mappedEvent);
}

/*! \reimp */
void QQuickWidget::showEvent(QShowEvent *)
{
    Q_D(QQuickWidget);
    if (!d->useSoftwareRenderer) {
        d->createContext();
        if (d->offscreenWindow->openglContext()) {
            d->render(true);
            // render() may have led to a QQuickWindow::update() call (for
            // example, having a scene with a QQuickFramebufferObject::Renderer
            // calling update() in its render()) which in turn results in
            // renderRequested in the rendercontrol, ending up in
            // triggerUpdate. In this case just calling update() is not
            // acceptable, we need the full renderSceneGraph issued from
            // timerEvent().
            if (!d->eventPending && d->updatePending) {
                d->updatePending = false;
                update();
            }
        } else {
            triggerUpdate();
        }
    }
    QWindowPrivate *offscreenPrivate = QWindowPrivate::get(d->offscreenWindow);
    if (!offscreenPrivate->visible) {
        offscreenPrivate->visible = true;
        emit d->offscreenWindow->visibleChanged(true);
        offscreenPrivate->updateVisibility();
    }
    if (QQmlInspectorService *service = QQmlDebugConnector::service<QQmlInspectorService>())
        service->setParentWindow(d->offscreenWindow, window()->windowHandle());
}

/*! \reimp */
void QQuickWidget::hideEvent(QHideEvent *)
{
    Q_D(QQuickWidget);
    if (!d->offscreenWindow->isPersistentSceneGraph())
        d->invalidateRenderControl();
    QWindowPrivate *offscreenPrivate = QWindowPrivate::get(d->offscreenWindow);
    if (offscreenPrivate->visible) {
        offscreenPrivate->visible = false;
        emit d->offscreenWindow->visibleChanged(false);
        offscreenPrivate->updateVisibility();
    }
    if (QQmlInspectorService *service = QQmlDebugConnector::service<QQmlInspectorService>())
        service->setParentWindow(d->offscreenWindow, d->offscreenWindow);
}

/*! \reimp */
void QQuickWidget::mousePressEvent(QMouseEvent *e)
{
    Q_D(QQuickWidget);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMousePress, e->button(),
                          e->buttons());

    QMouseEvent mappedEvent(e->type(), e->localPos(), e->screenPos(), e->button(), e->buttons(), e->modifiers());
    QCoreApplication::sendEvent(d->offscreenWindow, &mappedEvent);
    e->setAccepted(mappedEvent.isAccepted());
}

/*! \reimp */
void QQuickWidget::mouseReleaseEvent(QMouseEvent *e)
{
    Q_D(QQuickWidget);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMouseRelease, e->button(),
                          e->buttons());

    QMouseEvent mappedEvent(e->type(), e->localPos(), e->screenPos(), e->button(), e->buttons(), e->modifiers());
    QCoreApplication::sendEvent(d->offscreenWindow, &mappedEvent);
    e->setAccepted(mappedEvent.isAccepted());
}

#ifndef QT_NO_WHEELEVENT
/*! \reimp */
void QQuickWidget::wheelEvent(QWheelEvent *e)
{
    Q_D(QQuickWidget);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMouseWheel,
                          e->angleDelta().x(), e->angleDelta().y());

    // Wheel events only have local and global positions, no need to map.
    QCoreApplication::sendEvent(d->offscreenWindow, e);
}
#endif

/*!
   \reimp
*/
void QQuickWidget::focusInEvent(QFocusEvent * event)
{
    Q_D(QQuickWidget);
    d->offscreenWindow->focusInEvent(event);
}

/*!
   \reimp
*/
void QQuickWidget::focusOutEvent(QFocusEvent * event)
{
    Q_D(QQuickWidget);
    d->offscreenWindow->focusOutEvent(event);
}

static Qt::WindowState resolveWindowState(Qt::WindowStates states)
{
    // No more than one of these 3 can be set
    if (states & Qt::WindowMinimized)
        return Qt::WindowMinimized;
    if (states & Qt::WindowMaximized)
        return Qt::WindowMaximized;
    if (states & Qt::WindowFullScreen)
        return Qt::WindowFullScreen;

    // No state means "windowed" - we ignore Qt::WindowActive
    return Qt::WindowNoState;
}

/*! \reimp */
bool QQuickWidget::event(QEvent *e)
{
    Q_D(QQuickWidget);

    switch (e->type()) {
    case QEvent::InputMethod:
    case QEvent::InputMethodQuery:

    case QEvent::TouchBegin:
    case QEvent::TouchEnd:
    case QEvent::TouchUpdate:
    case QEvent::TouchCancel:
        // Touch events only have local and global positions, no need to map.
        return QCoreApplication::sendEvent(d->offscreenWindow, e);

    case QEvent::WindowChangeInternal:
        d->handleWindowChange();
        break;

    case QEvent::ScreenChangeInternal:
        if (QWindow *window = this->window()->windowHandle()) {
            QScreen *newScreen = window->screen();

            if (d->offscreenWindow)
                d->offscreenWindow->setScreen(newScreen);
            if (d->offscreenSurface)
                d->offscreenSurface->setScreen(newScreen);
#ifndef QT_NO_OPENGL
            if (d->context)
                d->context->setScreen(newScreen);
#endif
        }

        if (d->useSoftwareRenderer
#ifndef QT_NO_OPENGL
            || d->fbo
#endif
           ) {
            // This will check the size taking the devicePixelRatio into account
            // and recreate if needed.
            createFramebufferObject();
            d->render(true);
        }
        break;

    case QEvent::Show:
    case QEvent::Move:
        d->updatePosition();
        break;

    case QEvent::WindowStateChange:
        d->offscreenWindow->setWindowState(resolveWindowState(windowState()));
        break;

    default:
        break;
    }

    return QWidget::event(e);
}

#ifndef QT_NO_DRAGANDDROP

/*! \reimp */
void QQuickWidget::dragEnterEvent(QDragEnterEvent *e)
{
    Q_D(QQuickWidget);
    // Don't reject drag events for the entire widget when one
    // item rejects the drag enter
    d->offscreenWindow->event(e);
    e->accept();
}

/*! \reimp */
void QQuickWidget::dragMoveEvent(QDragMoveEvent *e)
{
    Q_D(QQuickWidget);
    // Drag/drop events only have local pos, so no need to map,
    // but QQuickWindow::event() does not return true
    d->offscreenWindow->event(e);
}

/*! \reimp */
void QQuickWidget::dragLeaveEvent(QDragLeaveEvent *e)
{
    Q_D(QQuickWidget);
    d->offscreenWindow->event(e);
}

/*! \reimp */
void QQuickWidget::dropEvent(QDropEvent *e)
{
    Q_D(QQuickWidget);
    d->offscreenWindow->event(e);
}

#endif // QT_NO_DRAGANDDROP

// TODO: try to separate the two cases of
// 1. render() unconditionally without sync
// 2. sync() and then render if necessary
void QQuickWidget::triggerUpdate()
{
    Q_D(QQuickWidget);
    d->updatePending = true;
     if (!d->eventPending) {
        // There's no sense in immediately kicking a render off now, as
        // there may be a number of triggerUpdate calls to come from a multitude
        // of different sources (network, touch/mouse/keyboard, timers,
        // animations, ...), and we want to batch them all into single frames as
        // much as possible for the sake of interactivity and responsiveness.
        //
        // To achieve this, we set a timer and only perform the rendering when
        // this is complete.
        const int exhaustDelay = 5;
        d->updateTimer.start(exhaustDelay, Qt::PreciseTimer, this);
        d->eventPending = true;
    }
}

/*!
    Sets the surface \a format for the context and offscreen surface used
    by this widget.

    Call this function when there is a need to request a context for a
    given OpenGL version or profile. The sizes for depth, stencil and
    alpha buffers are taken care of automatically and there is no need
    to request those explicitly.

    \sa QWindow::setFormat(), QWindow::format(), format()
*/
void QQuickWidget::setFormat(const QSurfaceFormat &format)
{
    Q_D(QQuickWidget);
    QSurfaceFormat currentFormat = d->offscreenWindow->format();
    QSurfaceFormat newFormat = format;
    newFormat.setDepthBufferSize(qMax(newFormat.depthBufferSize(), currentFormat.depthBufferSize()));
    newFormat.setStencilBufferSize(qMax(newFormat.stencilBufferSize(), currentFormat.stencilBufferSize()));
    newFormat.setAlphaBufferSize(qMax(newFormat.alphaBufferSize(), currentFormat.alphaBufferSize()));

    // Do not include the sample count. Requesting a multisampled context is not necessary
    // since we render into an FBO, never to an actual surface. What's more, attempting to
    // create a pbuffer with a multisampled config crashes certain implementations. Just
    // avoid the entire hassle, the result is the same.
    d->requestedSamples = newFormat.samples();
    newFormat.setSamples(0);

    d->offscreenWindow->setFormat(newFormat);
}

/*!
    Returns the actual surface format.

    If the widget has not yet been shown, the requested format is returned.

    \sa setFormat()
*/
QSurfaceFormat QQuickWidget::format() const
{
    Q_D(const QQuickWidget);
    return d->offscreenWindow->format();
}

/*!
  Renders a frame and reads it back into an image.

  \note This is a potentially expensive operation.
 */
QImage QQuickWidget::grabFramebuffer() const
{
    return const_cast<QQuickWidgetPrivate *>(d_func())->grabFramebuffer();
}

/*!
  Sets the clear \a color. By default this is an opaque color.

  To get a semi-transparent QQuickWidget, call this function with
  \a color set to Qt::transparent, set the Qt::WA_TranslucentBackground
  widget attribute on the top-level window, and request an alpha
  channel via setFormat().

  \sa QQuickWindow::setColor()
 */
void QQuickWidget::setClearColor(const QColor &color)
{
    Q_D(QQuickWidget);
    d->offscreenWindow->setColor(color);
}

/*!
    \since 5.5

    Returns the offscreen QQuickWindow which is used by this widget to drive
    the Qt Quick rendering. This is useful if you want to use QQuickWindow
    APIs that are not currently exposed by QQuickWidget, for instance
    connecting to the QQuickWindow::beforeRendering() signal in order
    to draw native OpenGL content below Qt Quick's own rendering.

    \warning Use the return value of this function with caution. In
    particular, do not ever attempt to show the QQuickWindow, and be
    very careful when using other QWindow-only APIs.
*/
QQuickWindow *QQuickWidget::quickWindow() const
{
    Q_D(const QQuickWidget);
    return d->offscreenWindow;
}

void QQuickWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    Q_D(QQuickWidget);
    if (d->useSoftwareRenderer) {
        QPainter painter(this);
        if (d->updateRegion.isNull()) {
            //Paint everything
            painter.drawImage(rect(), d->softwareImage);
        } else {
            //Paint only the updated areas
            const auto rects = d->updateRegion.rects();
            for (auto targetRect : rects) {
                auto sourceRect = QRect(targetRect.topLeft() * devicePixelRatio(), targetRect.size() * devicePixelRatio());
                painter.drawImage(targetRect, d->softwareImage, sourceRect);
            }
            d->updateRegion = QRegion();
        }
    }
}

QT_END_NAMESPACE
