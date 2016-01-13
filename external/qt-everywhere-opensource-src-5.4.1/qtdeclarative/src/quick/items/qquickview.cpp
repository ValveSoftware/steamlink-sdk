/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickview.h"
#include "qquickview_p.h"

#include "qquickwindow_p.h"
#include "qquickitem_p.h"
#include "qquickitemchangelistener_p.h"

#include <private/qquickprofiler_p.h>
#include <private/qqmlinspectorservice_p.h>
#include <private/qqmlmemoryprofiler_p.h>

#include <QtQml/qqmlengine.h>
#include <private/qqmlengine_p.h>
#include <QtCore/qbasictimer.h>

QT_BEGIN_NAMESPACE

DEFINE_OBJECT_VTABLE(QQuickRootItemMarker);

QQuickRootItemMarker *QQuickRootItemMarker::create(QQmlEngine *engine, QQuickWindow *window)
{
    QV4::ExecutionEngine *e = QQmlEnginePrivate::getV4Engine(engine);
    return e->memoryManager->alloc<QQuickRootItemMarker>(e, window);
}

void QQuickRootItemMarker::markObjects(QV4::Managed *that, QV4::ExecutionEngine *e)
{
    QQuickItem *root = static_cast<QQuickRootItemMarker*>(that)->d()->window->contentItem();
    if (root) {
        QQuickItemPrivate *rootPrivate = QQuickItemPrivate::get(root);
        rootPrivate->markObjects(e);
    }

    QV4::Object::markObjects(that, e);
}

void QQuickViewPrivate::init(QQmlEngine* e)
{
    Q_Q(QQuickView);

    engine = e;

    if (engine.isNull())
        engine = new QQmlEngine(q);

    if (!engine.data()->incubationController())
        engine.data()->setIncubationController(q->incubationController());

    {
        QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(engine.data());
        QV4::Scope scope(v4);
        QV4::Scoped<QQuickRootItemMarker> v(scope, QQuickRootItemMarker::create(engine.data(), q));
        rootItemMarker = v;
    }

    if (QQmlDebugService::isDebuggingEnabled())
        QQmlInspectorService::instance()->addView(q);
}

QQuickViewPrivate::QQuickViewPrivate()
    : root(0), component(0), resizeMode(QQuickView::SizeViewToRootObject), initialSize(0,0)
{
}

QQuickViewPrivate::~QQuickViewPrivate()
{
    if (QQmlDebugService::isDebuggingEnabled())
        QQmlInspectorService::instance()->removeView(q_func());
}

void QQuickViewPrivate::execute()
{
    Q_Q(QQuickView);
    if (!engine) {
        qWarning() << "QQuickView: invalid qml engine.";
        return;
    }

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

void QQuickViewPrivate::itemGeometryChanged(QQuickItem *resizeItem, const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_Q(QQuickView);
    if (resizeItem == root && resizeMode == QQuickView::SizeViewToRootObject) {
        // wait for both width and height to be changed
        resizetimer.start(0,q);
    }
    QQuickItemChangeListener::itemGeometryChanged(resizeItem, newGeometry, oldGeometry);
}

/*!
    \class QQuickView
    \since 5.0
    \brief The QQuickView class provides a window for displaying a Qt Quick user interface.

    \inmodule QtQuick

    This is a convenience subclass of QQuickWindow which
    will automatically load and display a QML scene when given the URL of the main source file. Alternatively,
    you can instantiate your own objects using QQmlComponent and place them in a manually setup QQuickWindow.

    Typical usage:

    \code
    QQuickView *view = new QQuickView;
    view->setSource(QUrl::fromLocalFile("myqmlfile.qml"));
    view->show();
    \endcode

    To receive errors related to loading and executing QML with QQuickView,
    you can connect to the statusChanged() signal and monitor for QQuickView::Error.
    The errors are available via QQuickView::errors().

    QQuickView also manages sizing of the view and root object.  By default, the \l resizeMode
    is SizeViewToRootObject, which will load the component and resize it to the
    size of the view.  Alternatively the resizeMode may be set to SizeRootObjectToView which
    will resize the view to the size of the root object.

    \sa {Exposing Attributes of C++ Types to QML}, QQuickWidget
*/


/*! \fn void QQuickView::statusChanged(QQuickView::Status status)
    This signal is emitted when the component's current \a status changes.
*/

/*!
  Constructs a QQuickView with the given \a parent.
  The default value of \a parent is 0.

*/
QQuickView::QQuickView(QWindow *parent)
: QQuickWindow(*(new QQuickViewPrivate), parent)
{
    d_func()->init();
}

/*!
  Constructs a QQuickView with the given QML \a source and \a parent.
  The default value of \a parent is 0.

*/
QQuickView::QQuickView(const QUrl &source, QWindow *parent)
: QQuickWindow(*(new QQuickViewPrivate), parent)
{
    d_func()->init();
    setSource(source);
}

/*!
  Constructs a QQuickView with the given QML \a engine and \a parent.

  Note: In this case, the QQuickView does not own the given \a engine object;
  it is the caller's responsibility to destroy the engine. If the \a engine is deleted
  before the view, status() will return QQuickView::Error.

  \sa Status, status(), errors()
*/
QQuickView::QQuickView(QQmlEngine* engine, QWindow *parent)
    : QQuickWindow(*(new QQuickViewPrivate), parent)
{
    Q_ASSERT(engine);
    d_func()->init(engine);
}

/*!
  Destroys the QQuickView.
*/
QQuickView::~QQuickView()
{
    // Ensure that the component is destroyed before the engine; the engine may
    // be a child of the QQuickViewPrivate, and will be destroyed by its dtor
    Q_D(QQuickView);
    delete d->root;
    d->root = 0;
}

/*!
  \property QQuickView::source
  \brief The URL of the source of the QML component.

  Ensure that the URL provided is full and correct, in particular, use
  \l QUrl::fromLocalFile() when loading a file from the local filesystem.

  Note that setting a source URL will result in the QML component being
  instantiated, even if the URL is unchanged from the current value.
*/

/*!
    Sets the source to the \a url, loads the QML component and instantiates it.

    Ensure that the URL provided is full and correct, in particular, use
    \l QUrl::fromLocalFile() when loading a file from the local filesystem.

    Calling this method multiple times with the same url will result
    in the QML component being reinstantiated.
 */
void QQuickView::setSource(const QUrl& url)
{
    Q_D(QQuickView);
    d->source = url;
    d->execute();
}

/*!
    \internal

    Set the source \a url, \a component and content \a item (root of the QML object hierarchy) directly.
 */
void QQuickView::setContent(const QUrl& url, QQmlComponent *component, QObject* item)
{
    Q_D(QQuickView);
    d->source = url;
    d->component = component;

    if (d->component && d->component->isError()) {
        QList<QQmlError> errorList = d->component->errors();
        foreach (const QQmlError &error, errorList) {
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
QUrl QQuickView::source() const
{
    Q_D(const QQuickView);
    return d->source;
}

/*!
  Returns a pointer to the QQmlEngine used for instantiating
  QML Components.
 */
QQmlEngine* QQuickView::engine() const
{
    Q_D(const QQuickView);
    return d->engine ? const_cast<QQmlEngine *>(d->engine.data()) : 0;
}

/*!
  This function returns the root of the context hierarchy.  Each QML
  component is instantiated in a QQmlContext.  QQmlContext's are
  essential for passing data to QML components.  In QML, contexts are
  arranged hierarchically and this hierarchy is managed by the
  QQmlEngine.
 */
QQmlContext* QQuickView::rootContext() const
{
    Q_D(const QQuickView);
    return d->engine ? d->engine.data()->rootContext() : 0;
}

/*!
    \enum QQuickView::Status
    Specifies the loading status of the QQuickView.

    \value Null This QQuickView has no source set.
    \value Ready This QQuickView has loaded and created the QML component.
    \value Loading This QQuickView is loading network data.
    \value Error One or more errors has occurred. Call errors() to retrieve a list
           of errors.
*/

/*! \enum QQuickView::ResizeMode

  This enum specifies how to resize the view.

  \value SizeViewToRootObject The view resizes with the root item in the QML.
  \value SizeRootObjectToView The view will automatically resize the root item to the size of the view.
*/

/*!
    \property QQuickView::status
    The component's current \l{QQuickView::Status} {status}.
*/

QQuickView::Status QQuickView::status() const
{
    Q_D(const QQuickView);
    if (!d->engine)
        return QQuickView::Error;

    if (!d->component)
        return QQuickView::Null;

    return QQuickView::Status(d->component->status());
}

/*!
    Return the list of errors that occurred during the last compile or create
    operation.  When the status is not Error, an empty list is returned.
*/
QList<QQmlError> QQuickView::errors() const
{
    Q_D(const QQuickView);
    QList<QQmlError> errs;

    if (d->component)
        errs = d->component->errors();

    if (!d->engine) {
        QQmlError error;
        error.setDescription(QLatin1String("QQuickView: invalid qml engine."));
        errs << error;
    }

    return errs;
}

/*!
    \property QQuickView::resizeMode
    \brief whether the view should resize the window contents

    If this property is set to SizeViewToRootObject (the default), the view
    resizes to the size of the root item in the QML.

    If this property is set to SizeRootObjectToView, the view will
    automatically resize the root item to the size of the view.

    \sa initialSize()
*/

void QQuickView::setResizeMode(ResizeMode mode)
{
    Q_D(QQuickView);
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

void QQuickViewPrivate::initResize()
{
    if (root) {
        if (resizeMode == QQuickView::SizeViewToRootObject) {
            QQuickItemPrivate *p = QQuickItemPrivate::get(root);
            p->addItemChangeListener(this, QQuickItemPrivate::Geometry);
        }
    }
    updateSize();
}

void QQuickViewPrivate::updateSize()
{
    Q_Q(QQuickView);
    if (!root)
        return;

    if (resizeMode == QQuickView::SizeViewToRootObject) {
        QSize newSize = QSize(root->width(), root->height());
        if (newSize.isValid() && newSize != q->size()) {
            q->resize(newSize);
        }
    } else if (resizeMode == QQuickView::SizeRootObjectToView) {
        if (!qFuzzyCompare(q->width(), root->width()))
            root->setWidth(q->width());
        if (!qFuzzyCompare(q->height(), root->height()))
            root->setHeight(q->height());
    }
}

QSize QQuickViewPrivate::rootObjectSize() const
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

QQuickView::ResizeMode QQuickView::resizeMode() const
{
    Q_D(const QQuickView);
    return d->resizeMode;
}

/*!
  \internal
 */
void QQuickView::continueExecute()
{
    Q_D(QQuickView);
    disconnect(d->component, SIGNAL(statusChanged(QQmlComponent::Status)), this, SLOT(continueExecute()));

    if (d->component->isError()) {
        QList<QQmlError> errorList = d->component->errors();
        foreach (const QQmlError &error, errorList) {
            QMessageLogger(error.url().toString().toLatin1().constData(), error.line(), 0).warning()
                    << error;
        }
        emit statusChanged(status());
        return;
    }

    QObject *obj = d->component->create();

    if (d->component->isError()) {
        QList<QQmlError> errorList = d->component->errors();
        foreach (const QQmlError &error, errorList) {
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
void QQuickViewPrivate::setRootObject(QObject *obj)
{
    Q_Q(QQuickView);
    if (root == obj)
        return;
    if (QQuickItem *sgItem = qobject_cast<QQuickItem *>(obj)) {
        root = sgItem;
        sgItem->setParentItem(q->QQuickWindow::contentItem());
    } else {
        qWarning() << "QQuickView only supports loading of root objects that derive from QQuickItem." << endl
                   << endl
                   << "If your example is using QML 2, (such as qmlscene) and the .qml file you" << endl
                   << "loaded has 'import QtQuick 1.0' or 'import Qt 4.7', this error will occur." << endl
                   << endl
                   << "To load files with 'import QtQuick 1.0' or 'import Qt 4.7', use the" << endl
                   << "QDeclarativeView class in the Qt Quick 1 module." << endl;
        delete obj;
        root = 0;
    }
    if (root) {
        initialSize = rootObjectSize();
        if ((resizeMode == QQuickView::SizeViewToRootObject || q->width() <= 1 || q->height() <= 1) &&
            initialSize != q->size()) {
            q->resize(initialSize);
        }
        initResize();
    }
}

/*!
  \internal
  If the \l {QTimerEvent} {timer event} \a e is this
  view's resize timer, sceneResized() is emitted.
 */
void QQuickView::timerEvent(QTimerEvent* e)
{
    Q_D(QQuickView);
    if (!e || e->timerId() == d->resizetimer.timerId()) {
        d->updateSize();
        d->resizetimer.stop();
    }
}

/*!
    \internal
    Preferred size follows the root object geometry.
*/
QSize QQuickView::sizeHint() const
{
    Q_D(const QQuickView);
    QSize rootObjectSize = d->rootObjectSize();
    if (rootObjectSize.isEmpty()) {
        return size();
    } else {
        return rootObjectSize;
    }
}

/*!
  Returns the initial size of the root object.

  If \l resizeMode is QQuickItem::SizeRootObjectToView the root object will be
  resized to the size of the view.  initialSize contains the size of the
  root object before it was resized.
*/
QSize QQuickView::initialSize() const
{
    Q_D(const QQuickView);
    return d->initialSize;
}

/*!
  Returns the view's root \l {QQuickItem} {item}.
 */
QQuickItem *QQuickView::rootObject() const
{
    Q_D(const QQuickView);
    return d->root;
}

/*!
  \internal
  This function handles the \l {QResizeEvent} {resize event}
  \a e.
 */
void QQuickView::resizeEvent(QResizeEvent *e)
{
    Q_D(QQuickView);
    if (d->resizeMode == SizeRootObjectToView)
        d->updateSize();

    QQuickWindow::resizeEvent(e);
}

/*! \reimp */
void QQuickView::keyPressEvent(QKeyEvent *e)
{
    Q_QUICK_INPUT_PROFILE(addEvent<QQuickProfiler::Key>());

    QQuickWindow::keyPressEvent(e);
}

/*! \reimp */
void QQuickView::keyReleaseEvent(QKeyEvent *e)
{
    Q_QUICK_INPUT_PROFILE(addEvent<QQuickProfiler::Key>());

    QQuickWindow::keyReleaseEvent(e);
}

/*! \reimp */
void QQuickView::mouseMoveEvent(QMouseEvent *e)
{
    Q_QUICK_INPUT_PROFILE(addEvent<QQuickProfiler::Mouse>());

    QQuickWindow::mouseMoveEvent(e);
}

/*! \reimp */
void QQuickView::mousePressEvent(QMouseEvent *e)
{
    Q_QUICK_INPUT_PROFILE(addEvent<QQuickProfiler::Mouse>());

    QQuickWindow::mousePressEvent(e);
}

/*! \reimp */
void QQuickView::mouseReleaseEvent(QMouseEvent *e)
{
    Q_QUICK_INPUT_PROFILE(addEvent<QQuickProfiler::Mouse>());

    QQuickWindow::mouseReleaseEvent(e);
}


QT_END_NAMESPACE
