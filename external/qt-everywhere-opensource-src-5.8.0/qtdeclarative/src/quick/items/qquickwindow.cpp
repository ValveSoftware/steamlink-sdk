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

#include "qquickwindow.h"
#include "qquickwindow_p.h"
#include "qquickwindowattached_p.h"

#include "qquickitem.h"
#include "qquickitem_p.h"
#include "qquickevents_p_p.h"

#include <private/qquickdrag_p.h>

#include <QtQuick/private/qsgrenderer_p.h>
#include <QtQuick/private/qsgtexture_p.h>
#include <private/qsgrenderloop_p.h>
#include <private/qquickrendercontrol_p.h>
#include <private/qquickanimatorcontroller_p.h>
#include <private/qquickprofiler_p.h>

#include <private/qguiapplication_p.h>
#include <QtGui/QInputMethod>

#include <private/qabstractanimation_p.h>

#include <QtGui/qpainter.h>
#include <QtGui/qevent.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qstylehints.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qabstractanimation.h>
#include <QtCore/QLibraryInfo>
#include <QtCore/QRunnable>
#include <QtQml/qqmlincubator.h>

#include <QtQuick/private/qquickpixmapcache_p.h>

#include <private/qqmlmemoryprofiler_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qqmldebugconnector_p.h>
#ifndef QT_NO_OPENGL
# include <private/qopenglvertexarrayobject_p.h>
# include <private/qsgdefaultrendercontext_p.h>
#endif

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(DBG_TOUCH, "qt.quick.touch")
Q_LOGGING_CATEGORY(DBG_TOUCH_TARGET, "qt.quick.touch.target")
Q_LOGGING_CATEGORY(DBG_MOUSE, "qt.quick.mouse")
Q_LOGGING_CATEGORY(DBG_MOUSE_TARGET, "qt.quick.mouse.target")
Q_LOGGING_CATEGORY(DBG_HOVER_TRACE, "qt.quick.hover.trace")
Q_LOGGING_CATEGORY(DBG_FOCUS, "qt.quick.focus")
Q_LOGGING_CATEGORY(DBG_DIRTY, "qt.quick.dirty")

extern Q_GUI_EXPORT QImage qt_gl_read_framebuffer(const QSize &size, bool alpha_format, bool include_alpha);

bool QQuickWindowPrivate::defaultAlphaBuffer = false;

void QQuickWindowPrivate::updateFocusItemTransform()
{
#ifndef QT_NO_IM
    Q_Q(QQuickWindow);
    QQuickItem *focus = q->activeFocusItem();
    if (focus && QGuiApplication::focusObject() == focus) {
        QQuickItemPrivate *focusPrivate = QQuickItemPrivate::get(focus);
        QGuiApplication::inputMethod()->setInputItemTransform(focusPrivate->itemToWindowTransform());
        QGuiApplication::inputMethod()->setInputItemRectangle(QRectF(0, 0, focusPrivate->width, focusPrivate->height));
        focus->updateInputMethod(Qt::ImInputItemClipRectangle);
    }
#endif
}

class QQuickWindowIncubationController : public QObject, public QQmlIncubationController
{
    Q_OBJECT

public:
    QQuickWindowIncubationController(QSGRenderLoop *loop)
        : m_renderLoop(loop), m_timer(0)
    {
        // Allow incubation for 1/3 of a frame.
        m_incubation_time = qMax(1, int(1000 / QGuiApplication::primaryScreen()->refreshRate()) / 3);

        QAnimationDriver *animationDriver = m_renderLoop->animationDriver();
        if (animationDriver) {
            connect(animationDriver, SIGNAL(stopped()), this, SLOT(animationStopped()));
            connect(m_renderLoop, SIGNAL(timeToIncubate()), this, SLOT(incubate()));
        }
    }

protected:
    void timerEvent(QTimerEvent *) Q_DECL_OVERRIDE
    {
        killTimer(m_timer);
        m_timer = 0;
        incubate();
    }

    void incubateAgain() {
        if (m_timer == 0) {
            // Wait for a while before processing the next batch. Using a
            // timer to avoid starvation of system events.
            m_timer = startTimer(m_incubation_time);
        }
    }

public slots:
    void incubate() {
        if (incubatingObjectCount()) {
            if (m_renderLoop->interleaveIncubation()) {
                incubateFor(m_incubation_time);
            } else {
                incubateFor(m_incubation_time * 2);
                if (incubatingObjectCount())
                    incubateAgain();
            }
        }
    }

    void animationStopped() { incubate(); }

protected:
    void incubatingObjectCountChanged(int count) Q_DECL_OVERRIDE
    {
        if (count && !m_renderLoop->interleaveIncubation())
            incubateAgain();
    }

private:
    QSGRenderLoop *m_renderLoop;
    int m_incubation_time;
    int m_timer;
};

#include "qquickwindow.moc"


#ifndef QT_NO_ACCESSIBILITY
/*!
    Returns an accessibility interface for this window, or 0 if such an
    interface cannot be created.
*/
QAccessibleInterface *QQuickWindow::accessibleRoot() const
{
    return QAccessible::queryAccessibleInterface(const_cast<QQuickWindow*>(this));
}
#endif


/*
Focus behavior
==============

Prior to being added to a valid window items can set and clear focus with no
effect.  Only once items are added to a window (by way of having a parent set that
already belongs to a window) do the focus rules apply.  Focus goes back to
having no effect if an item is removed from a window.

When an item is moved into a new focus scope (either being added to a window
for the first time, or having its parent changed), if the focus scope already has
a scope focused item that takes precedence over the item being added.  Otherwise,
the focus of the added tree is used.  In the case of a tree of items being
added to a window for the first time, which may have a conflicted focus state (two
or more items in one scope having focus set), the same rule is applied item by item -
thus the first item that has focus will get it (assuming the scope doesn't already
have a scope focused item), and the other items will have their focus cleared.
*/

QQuickRootItem::QQuickRootItem()
{
}

/*! \reimp */
void QQuickWindow::exposeEvent(QExposeEvent *)
{
    Q_D(QQuickWindow);
    if (d->windowManager)
        d->windowManager->exposureChanged(this);
}

/*! \reimp */
void QQuickWindow::resizeEvent(QResizeEvent *ev)
{
    Q_D(QQuickWindow);
    if (d->contentItem)
        d->contentItem->setSize(ev->size());
    if (d->windowManager)
        d->windowManager->resize(this);
}

/*! \reimp */
void QQuickWindow::showEvent(QShowEvent *)
{
    Q_D(QQuickWindow);
    if (d->windowManager)
        d->windowManager->show(this);
}

/*! \reimp */
void QQuickWindow::hideEvent(QHideEvent *)
{
    Q_D(QQuickWindow);
    if (d->windowManager)
        d->windowManager->hide(this);
}

/*! \reimp */
void QQuickWindow::focusOutEvent(QFocusEvent *ev)
{
    Q_D(QQuickWindow);
    d->contentItem->setFocus(false, ev->reason());
}

/*! \reimp */
void QQuickWindow::focusInEvent(QFocusEvent *ev)
{
    Q_D(QQuickWindow);
    d->contentItem->setFocus(true, ev->reason());
    d->updateFocusItemTransform();
}

#ifndef QT_NO_IM
static bool transformDirtyOnItemOrAncestor(const QQuickItem *item)
{
    while (item) {
        if (QQuickItemPrivate::get(item)->dirtyAttributes & (
            QQuickItemPrivate::TransformOrigin |
            QQuickItemPrivate::Transform |
            QQuickItemPrivate::BasicTransform |
            QQuickItemPrivate::Position |
            QQuickItemPrivate::Size |
            QQuickItemPrivate::ParentChanged |
            QQuickItemPrivate::Clip)) {
            return true;
        }
        item = item->parentItem();
    }
    return false;
}
#endif

void QQuickWindowPrivate::polishItems()
{
    // An item can trigger polish on another item, or itself for that matter,
    // during its updatePolish() call. Because of this, we cannot simply
    // iterate through the set, we must continue pulling items out until it
    // is empty.
    // In the case where polish is called from updatePolish() either directly
    // or indirectly, we use a recursionSafeguard to print a warning to
    // the user.
    int recursionSafeguard = INT_MAX;
    while (!itemsToPolish.isEmpty() && --recursionSafeguard > 0) {
        QQuickItem *item = itemsToPolish.takeLast();
        QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
        itemPrivate->polishScheduled = false;
        itemPrivate->updatePolish();
        item->updatePolish();
    }

    if (recursionSafeguard == 0)
        qWarning("QQuickWindow: possible QQuickItem::polish() loop");

#ifndef QT_NO_IM
    if (QQuickItem *focusItem = q_func()->activeFocusItem()) {
        // If the current focus item, or any of its anchestors, has changed location
        // inside the window, we need inform IM about it. This to ensure that overlays
        // such as selection handles will be updated.
        const bool isActiveFocusItem = (focusItem == QGuiApplication::focusObject());
        const bool hasImEnabled = focusItem->inputMethodQuery(Qt::ImEnabled).toBool();
        if (isActiveFocusItem && hasImEnabled && transformDirtyOnItemOrAncestor(focusItem))
            updateFocusItemTransform();
    }
#endif
}

/*!
 * Schedules the window to render another frame.
 *
 * Calling QQuickWindow::update() differs from QQuickItem::update() in that
 * it always triggers a repaint, regardless of changes in the underlying
 * scene graph or not.
 */
void QQuickWindow::update()
{
    Q_D(QQuickWindow);
    if (d->windowManager)
        d->windowManager->update(this);
    else if (d->renderControl)
        QQuickRenderControlPrivate::get(d->renderControl)->update();
}

static void updatePixelRatioHelper(QQuickItem *item, float pixelRatio)
{
    if (item->flags() & QQuickItem::ItemHasContents) {
        QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
        itemPrivate->itemChange(QQuickItem::ItemDevicePixelRatioHasChanged, pixelRatio);
    }

    QList <QQuickItem *> items = item->childItems();
    for (int i = 0; i < items.size(); ++i)
        updatePixelRatioHelper(items.at(i), pixelRatio);
}

void QQuickWindow::physicalDpiChanged()
{
    Q_D(QQuickWindow);
    const qreal newPixelRatio = screen()->devicePixelRatio();
    if (qFuzzyCompare(newPixelRatio, d->devicePixelRatio))
        return;
    d->devicePixelRatio = newPixelRatio;
    if (d->contentItem)
        updatePixelRatioHelper(d->contentItem, newPixelRatio);
}

void QQuickWindow::handleScreenChanged(QScreen *screen)
{
    Q_D(QQuickWindow);
    if (screen) {
        physicalDpiChanged();
        // When physical DPI changes on the same screen, either the resolution or the device pixel
        // ratio changed. We must check what it is. Device pixel ratio does not have its own
        // ...Changed() signal.
        d->physicalDpiChangedConnection = connect(screen, SIGNAL(physicalDotsPerInchChanged(qreal)),
                                                  this, SLOT(physicalDpiChanged()));
    } else {
        disconnect(d->physicalDpiChangedConnection);
    }

    d->forcePolish();
}

void forcePolishHelper(QQuickItem *item)
{
    if (item->flags() & QQuickItem::ItemHasContents) {
        item->polish();
    }

    QList <QQuickItem *> items = item->childItems();
    for (int i=0; i<items.size(); ++i)
        forcePolishHelper(items.at(i));
}

/*!
    Schedules polish events on all items in the scene.
*/
void QQuickWindowPrivate::forcePolish()
{
    Q_Q(QQuickWindow);
    if (!q->screen())
        return;
    forcePolishHelper(contentItem);
}

void forceUpdate(QQuickItem *item)
{
    if (item->flags() & QQuickItem::ItemHasContents)
        item->update();
    QQuickItemPrivate::get(item)->dirty(QQuickItemPrivate::ChildrenUpdateMask);

    QList <QQuickItem *> items = item->childItems();
    for (int i=0; i<items.size(); ++i)
        forceUpdate(items.at(i));
}

void QQuickWindowPrivate::syncSceneGraph()
{
    QML_MEMORY_SCOPE_STRING("SceneGraph");
    Q_Q(QQuickWindow);

    animationController->beforeNodeSync();

    emit q->beforeSynchronizing();
    runAndClearJobs(&beforeSynchronizingJobs);
    if (!renderer) {
        forceUpdate(contentItem);

        QSGRootNode *rootNode = new QSGRootNode;
        rootNode->appendChildNode(QQuickItemPrivate::get(contentItem)->itemNode());
        renderer = context->createRenderer();
        renderer->setRootNode(rootNode);
    }

    updateDirtyNodes();

    animationController->afterNodeSync();

    // Copy the current state of clearing from window into renderer.
    renderer->setClearColor(clearColor);
    QSGAbstractRenderer::ClearMode mode = QSGAbstractRenderer::ClearStencilBuffer | QSGAbstractRenderer::ClearDepthBuffer;
    if (clearBeforeRendering)
        mode |= QSGAbstractRenderer::ClearColorBuffer;
    renderer->setClearMode(mode);

    renderer->setCustomRenderMode(customRenderMode);

    emit q->afterSynchronizing();
    runAndClearJobs(&afterSynchronizingJobs);
    context->endSync();
}


void QQuickWindowPrivate::renderSceneGraph(const QSize &size)
{
    QML_MEMORY_SCOPE_STRING("SceneGraph");
    Q_Q(QQuickWindow);
    if (!renderer)
        return;

    animationController->advance();
    emit q->beforeRendering();
    runAndClearJobs(&beforeRenderingJobs);
    if (!customRenderStage || !customRenderStage->render()) {
        int fboId = 0;
        const qreal devicePixelRatio = q->effectiveDevicePixelRatio();
        if (renderTargetId) {
            QRect rect(QPoint(0, 0), renderTargetSize);
            fboId = renderTargetId;
            renderer->setDeviceRect(rect);
            renderer->setViewportRect(rect);
        } else {
            QRect rect(QPoint(0, 0), devicePixelRatio * size);
            renderer->setDeviceRect(rect);
            renderer->setViewportRect(rect);
        }
        renderer->setProjectionMatrixToRect(QRect(QPoint(0, 0), size));
        renderer->setDevicePixelRatio(devicePixelRatio);

        context->renderNextFrame(renderer, fboId);
    }
    emit q->afterRendering();
    runAndClearJobs(&afterRenderingJobs);
}

QQuickWindowPrivate::QQuickWindowPrivate()
    : contentItem(0)
    , activeFocusItem(0)
#ifndef QT_NO_CURSOR
    , cursorItem(0)
#endif
#ifndef QT_NO_DRAGANDDROP
    , dragGrabber(0)
#endif
    , touchMouseId(-1)
    , touchMouseDevice(nullptr)
    , touchMousePressTimestamp(0)
    , dirtyItemList(0)
    , devicePixelRatio(0)
    , context(0)
    , renderer(0)
    , windowManager(0)
    , renderControl(0)
    , pointerEventRecursionGuard(0)
    , customRenderStage(0)
    , clearColor(Qt::white)
    , clearBeforeRendering(true)
    , persistentGLContext(true)
    , persistentSceneGraph(true)
    , lastWheelEventAccepted(false)
    , componentCompleted(true)
    , lastFocusReason(Qt::OtherFocusReason)
    , renderTarget(0)
    , renderTargetId(0)
    , vaoHelper(0)
    , incubationController(0)
{
#ifndef QT_NO_DRAGANDDROP
    dragGrabber = new QQuickDragGrabber;
#endif
}

QQuickWindowPrivate::~QQuickWindowPrivate()
{
    delete customRenderStage;
    if (QQmlInspectorService *service = QQmlDebugConnector::service<QQmlInspectorService>())
        service->removeWindow(q_func());
}

void QQuickWindowPrivate::init(QQuickWindow *c, QQuickRenderControl *control)
{
    q_ptr = c;

    Q_Q(QQuickWindow);

    contentItem = new QQuickRootItem;
    QQmlEngine::setObjectOwnership(contentItem, QQmlEngine::CppOwnership);
    QQuickItemPrivate *contentItemPrivate = QQuickItemPrivate::get(contentItem);
    contentItemPrivate->window = q;
    contentItemPrivate->windowRefCount = 1;
    contentItemPrivate->flags |= QQuickItem::ItemIsFocusScope;
    contentItem->setSize(q->size());

    customRenderMode = qgetenv("QSG_VISUALIZE");
    renderControl = control;
    if (renderControl)
        QQuickRenderControlPrivate::get(renderControl)->window = q;

    if (!renderControl)
        windowManager = QSGRenderLoop::instance();

    Q_ASSERT(windowManager || renderControl);

    if (QScreen *screen = q->screen())
       devicePixelRatio = screen->devicePixelRatio();

    QSGContext *sg;
    if (renderControl) {
        QQuickRenderControlPrivate *renderControlPriv = QQuickRenderControlPrivate::get(renderControl);
        sg = renderControlPriv->sg;
        context = renderControlPriv->rc;
    } else {
        windowManager->addWindow(q);
        sg = windowManager->sceneGraphContext();
        context = windowManager->createRenderContext(sg);
    }

    q->setSurfaceType(windowManager ? windowManager->windowSurfaceType() : QSurface::OpenGLSurface);
    q->setFormat(sg->defaultSurfaceFormat());

    animationController = new QQuickAnimatorController(q);

    QObject::connect(context, SIGNAL(initialized()), q, SIGNAL(sceneGraphInitialized()), Qt::DirectConnection);
    QObject::connect(context, SIGNAL(invalidated()), q, SIGNAL(sceneGraphInvalidated()), Qt::DirectConnection);
    QObject::connect(context, SIGNAL(invalidated()), q, SLOT(cleanupSceneGraph()), Qt::DirectConnection);

    QObject::connect(q, SIGNAL(focusObjectChanged(QObject*)), q, SIGNAL(activeFocusItemChanged()));
    QObject::connect(q, SIGNAL(screenChanged(QScreen*)), q, SLOT(handleScreenChanged(QScreen*)));

    QObject::connect(q, SIGNAL(frameSwapped()), q, SLOT(runJobsAfterSwap()), Qt::DirectConnection);

    if (QQmlInspectorService *service = QQmlDebugConnector::service<QQmlInspectorService>())
        service->addWindow(q);
}

/*!
    \property QQuickWindow::data
    \internal
*/

QQmlListProperty<QObject> QQuickWindowPrivate::data()
{
    return QQmlListProperty<QObject>(q_func(), 0, QQuickWindowPrivate::data_append,
                                             QQuickWindowPrivate::data_count,
                                             QQuickWindowPrivate::data_at,
                                             QQuickWindowPrivate::data_clear);
}

static QMouseEvent *touchToMouseEvent(QEvent::Type type, const QTouchEvent::TouchPoint &p, QTouchEvent *event, QQuickItem *item, bool transformNeeded = true)
{
    // The touch point local position and velocity are not yet transformed.
    QMouseEvent *me = new QMouseEvent(type, transformNeeded ? item->mapFromScene(p.scenePos()) : p.pos(), p.scenePos(), p.screenPos(),
                                      Qt::LeftButton, (type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton), event->modifiers());
    me->setAccepted(true);
    me->setTimestamp(event->timestamp());
    QVector2D transformedVelocity = p.velocity();
    if (transformNeeded) {
        QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
        QMatrix4x4 transformMatrix(itemPrivate->windowToItemTransform());
        transformedVelocity = transformMatrix.mapVector(p.velocity()).toVector2D();
    }
    QGuiApplicationPrivate::setMouseEventCapsAndVelocity(me, event->device()->capabilities(), transformedVelocity);
    QGuiApplicationPrivate::setMouseEventSource(me, Qt::MouseEventSynthesizedByQt);
    return me;
}

bool QQuickWindowPrivate::checkIfDoubleClicked(ulong newPressEventTimestamp)
{
    bool doubleClicked;

    if (touchMousePressTimestamp == 0) {
        // just initialize the variable
        touchMousePressTimestamp = newPressEventTimestamp;
        doubleClicked = false;
    } else {
        ulong timeBetweenPresses = newPressEventTimestamp - touchMousePressTimestamp;
        ulong doubleClickInterval = static_cast<ulong>(QGuiApplication::styleHints()->
                mouseDoubleClickInterval());
        doubleClicked = timeBetweenPresses < doubleClickInterval;
        if (doubleClicked) {
            touchMousePressTimestamp = 0;
        } else {
            touchMousePressTimestamp = newPressEventTimestamp;
        }
    }

    return doubleClicked;
}

bool QQuickWindowPrivate::deliverTouchAsMouse(QQuickItem *item, QQuickPointerEvent *pointerEvent)
{
    Q_Q(QQuickWindow);
    auto device = pointerEvent->device();

    // FIXME: make this work for mouse events too and get rid of the asTouchEvent in here.
    Q_ASSERT(pointerEvent->asPointerTouchEvent());
    QTouchEvent *event = pointerEvent->asPointerTouchEvent()->touchEventForItem(item);
    if (!event)
        return false;

    // For each point, check if it is accepted, if not, try the next point.
    // Any of the fingers can become the mouse one.
    // This can happen because a mouse area might not accept an event at some point but another.
    for (int i = 0; i < event->touchPoints().count(); ++i) {
        const QTouchEvent::TouchPoint &p = event->touchPoints().at(i);
        // A new touch point
        if (touchMouseId == -1 && p.state() & Qt::TouchPointPressed) {
            QPointF pos = item->mapFromScene(p.scenePos());

            // probably redundant, we check bounds in the calling function (matchingNewPoints)
            if (!item->contains(pos))
                break;

            qCDebug(DBG_TOUCH_TARGET) << "TP (mouse)" << p.id() << "->" << item;
            QScopedPointer<QMouseEvent> mousePress(touchToMouseEvent(QEvent::MouseButtonPress, p, event, item, false));

            // Send a single press and see if that's accepted
            QCoreApplication::sendEvent(item, mousePress.data());
            event->setAccepted(mousePress->isAccepted());
            if (mousePress->isAccepted()) {
                touchMouseDevice = device;
                touchMouseId = p.id();
                if (!q->mouseGrabberItem())
                    item->grabMouse();
                auto pointerEventPoint = pointerEvent->pointById(p.id());
                pointerEventPoint->setGrabber(item);

                if (checkIfDoubleClicked(event->timestamp())) {
                    QScopedPointer<QMouseEvent> mouseDoubleClick(touchToMouseEvent(QEvent::MouseButtonDblClick, p, event, item, false));
                    QCoreApplication::sendEvent(item, mouseDoubleClick.data());
                    event->setAccepted(mouseDoubleClick->isAccepted());
                    if (!mouseDoubleClick->isAccepted()) {
                        touchMouseId = -1;
                        touchMouseDevice = nullptr;
                    }
                }

                return true;
            }
            // try the next point

        // Touch point was there before and moved
        } else if (touchMouseDevice == device && p.id() == touchMouseId) {
            if (p.state() & Qt::TouchPointMoved) {
                if (QQuickItem *mouseGrabberItem = q->mouseGrabberItem()) {
                    QScopedPointer<QMouseEvent> me(touchToMouseEvent(QEvent::MouseMove, p, event, mouseGrabberItem, false));
                    QCoreApplication::sendEvent(item, me.data());
                    event->setAccepted(me->isAccepted());
                    if (me->isAccepted()) {
                        qCDebug(DBG_TOUCH_TARGET) << "TP (mouse)" << p.id() << "->" << mouseGrabberItem;
                    }
                    return event->isAccepted();
                } else {
                    // no grabber, check if we care about mouse hover
                    // FIXME: this should only happen once, not recursively... I'll ignore it just ignore hover now.
                    // hover for touch???
                    QScopedPointer<QMouseEvent> me(touchToMouseEvent(QEvent::MouseMove, p, event, item, false));
                    if (lastMousePosition.isNull())
                        lastMousePosition = me->windowPos();
                    QPointF last = lastMousePosition;
                    lastMousePosition = me->windowPos();

                    bool accepted = me->isAccepted();
                    bool delivered = deliverHoverEvent(contentItem, me->windowPos(), last, me->modifiers(), me->timestamp(), accepted);
                    if (!delivered) {
                        //take care of any exits
                        accepted = clearHover(me->timestamp());
                    }
                    me->setAccepted(accepted);
                    break;
                }
            } else if (p.state() & Qt::TouchPointReleased) {
                // currently handled point was released
                if (QQuickItem *mouseGrabberItem = q->mouseGrabberItem()) {
                    QScopedPointer<QMouseEvent> me(touchToMouseEvent(QEvent::MouseButtonRelease, p, event, mouseGrabberItem, false));
                    QCoreApplication::sendEvent(item, me.data());

                    if (item->acceptHoverEvents() && p.screenPos() != QGuiApplicationPrivate::lastCursorPosition) {
                        QPointF localMousePos(qInf(), qInf());
                        if (QWindow *w = item->window())
                            localMousePos = item->mapFromScene(w->mapFromGlobal(QGuiApplicationPrivate::lastCursorPosition.toPoint()));
                        QMouseEvent mm(QEvent::MouseMove, localMousePos, QGuiApplicationPrivate::lastCursorPosition,
                                       Qt::NoButton, Qt::NoButton, event->modifiers());
                        QCoreApplication::sendEvent(item, &mm);
                    }
                    if (q->mouseGrabberItem()) // might have ungrabbed due to event
                        q->mouseGrabberItem()->ungrabMouse();

                    touchMouseId = -1;
                    touchMouseDevice = nullptr;
                    return me->isAccepted();
                }
            }
            break;
        }
    }
    return false;
}

void QQuickWindowPrivate::setMouseGrabber(QQuickItem *grabber)
{
    Q_Q(QQuickWindow);
    if (q->mouseGrabberItem() == grabber)
        return;

    qCDebug(DBG_MOUSE_TARGET) << "grabber" << q->mouseGrabberItem() << "->" << grabber;
    QQuickItem *oldGrabber = q->mouseGrabberItem();

    if (grabber && touchMouseId != -1 && touchMouseDevice) {
        // update the touch item for mouse touch id to the new grabber
        qCDebug(DBG_TOUCH_TARGET) << "TP (mouse)" << touchMouseId << "->" << q->mouseGrabberItem();
        auto point = touchMouseDevice->pointerEvent()->pointById(touchMouseId);
        if (point)
            point->setGrabber(grabber);
    } else {
        QQuickPointerEvent *event = QQuickPointerDevice::genericMouseDevice()->pointerEvent();
        Q_ASSERT(event->pointCount() == 1);
        event->point(0)->setGrabber(grabber);
    }

    if (oldGrabber) {
        QEvent e(QEvent::UngrabMouse);
        QSet<QQuickItem *> hasFiltered;
        if (!sendFilteredMouseEvent(oldGrabber->parentItem(), oldGrabber, &e, &hasFiltered))
            oldGrabber->mouseUngrabEvent();
    }
}

void QQuickWindowPrivate::grabTouchPoints(QQuickItem *grabber, const QVector<int> &ids)
{
    QSet<QQuickItem*> ungrab;
    for (int i = 0; i < ids.count(); ++i) {
        // FIXME: deprecate this function, we need a device
        int id = ids.at(i);
        if (Q_UNLIKELY(id < 0)) {
            qWarning("ignoring grab of touchpoint %d", id);
            continue;
        }
        if (id == touchMouseId) {
            auto point = touchMouseDevice->pointerEvent()->pointById(id);
            auto touchMouseGrabber = point->grabber();
            if (touchMouseGrabber) {
                point->setGrabber(nullptr);
                touchMouseGrabber->mouseUngrabEvent();
                ungrab.insert(touchMouseGrabber);
                touchMouseDevice = nullptr;
                touchMouseId = -1;
            }
            qCDebug(DBG_MOUSE_TARGET) << "grabTouchPoints: mouse grabber changed due to grabTouchPoints:" << touchMouseGrabber << "-> null";
        }

        const auto touchDevices = QQuickPointerDevice::touchDevices();
        for (auto device : touchDevices) {
            auto point = device->pointerEvent()->pointById(id);
            if (!point)
                continue;
            QQuickItem *oldGrabber = point->grabber();
            if (oldGrabber == grabber)
                continue;

            point->setGrabber(grabber);
            if (oldGrabber)
                ungrab.insert(oldGrabber);
        }
    }
    for (QQuickItem *oldGrabber : qAsConst(ungrab))
        oldGrabber->touchUngrabEvent();
}

void QQuickWindowPrivate::removeGrabber(QQuickItem *grabber, bool mouse, bool touch)
{
    Q_Q(QQuickWindow);
    if (Q_LIKELY(touch)) {
        const auto touchDevices = QQuickPointerDevice::touchDevices();
        for (auto device : touchDevices) {
            auto pointerEvent = device->pointerEvent();
            for (int i = 0; i < pointerEvent->pointCount(); ++i) {
                if (pointerEvent->point(i)->grabber() == grabber) {
                    pointerEvent->point(i)->setGrabber(nullptr);
                    // FIXME send ungrab event only once
                    grabber->touchUngrabEvent();
                }
            }
        }
    }
    if (Q_LIKELY(mouse) && q->mouseGrabberItem() == grabber) {
        qCDebug(DBG_MOUSE_TARGET) << "removeGrabber" << q->mouseGrabberItem() << "-> null";
        setMouseGrabber(nullptr);
    }
}

/*!
Translates the data in \a touchEvent to this window.  This method leaves the item local positions in
\a touchEvent untouched (these are filled in later).
*/
void QQuickWindowPrivate::translateTouchEvent(QTouchEvent *touchEvent)
{
    QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
    for (int i = 0; i < touchPoints.count(); ++i) {
        QTouchEvent::TouchPoint &touchPoint = touchPoints[i];

        touchPoint.setSceneRect(touchPoint.rect());
        touchPoint.setStartScenePos(touchPoint.startPos());
        touchPoint.setLastScenePos(touchPoint.lastPos());
    }
    touchEvent->setTouchPoints(touchPoints);
}


static inline bool windowHasFocus(QQuickWindow *win)
{
    const QWindow *focusWindow = QGuiApplication::focusWindow();
    return win == focusWindow || QQuickRenderControl::renderWindowFor(win) == focusWindow;
}

/*!
Set the focus inside \a scope to be \a item.
If the scope contains the active focus item, it will be changed to \a item.
Calls notifyFocusChangesRecur for all changed items.
*/
void QQuickWindowPrivate::setFocusInScope(QQuickItem *scope, QQuickItem *item, Qt::FocusReason reason, FocusOptions options)
{
    Q_Q(QQuickWindow);

    Q_ASSERT(item);
    Q_ASSERT(scope || item == contentItem);

    qCDebug(DBG_FOCUS) << "QQuickWindowPrivate::setFocusInScope():";
    qCDebug(DBG_FOCUS) << "    scope:" << (QObject *)scope;
    if (scope)
        qCDebug(DBG_FOCUS) << "    scopeSubFocusItem:" << (QObject *)QQuickItemPrivate::get(scope)->subFocusItem;
    qCDebug(DBG_FOCUS) << "    item:" << (QObject *)item;
    qCDebug(DBG_FOCUS) << "    activeFocusItem:" << (QObject *)activeFocusItem;

    QQuickItemPrivate *scopePrivate = scope ? QQuickItemPrivate::get(scope) : 0;
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

    QQuickItem *oldActiveFocusItem = 0;
    QQuickItem *currentActiveFocusItem = activeFocusItem;
    QQuickItem *newActiveFocusItem = 0;
    bool sendFocusIn = false;

    lastFocusReason = reason;

    QVarLengthArray<QQuickItem *, 20> changed;

    // Does this change the active focus?
    if (item == contentItem || scopePrivate->activeFocus) {
        oldActiveFocusItem = activeFocusItem;
        if (item->isEnabled()) {
            newActiveFocusItem = item;
            while (newActiveFocusItem->isFocusScope()
                   && newActiveFocusItem->scopedFocusItem()
                   && newActiveFocusItem->scopedFocusItem()->isEnabled()) {
                newActiveFocusItem = newActiveFocusItem->scopedFocusItem();
            }
        } else {
            newActiveFocusItem = scope;
        }

        if (oldActiveFocusItem) {
#ifndef QT_NO_IM
            QGuiApplication::inputMethod()->commit();
#endif

            activeFocusItem = 0;

            QQuickItem *afi = oldActiveFocusItem;
            while (afi && afi != scope) {
                if (QQuickItemPrivate::get(afi)->activeFocus) {
                    QQuickItemPrivate::get(afi)->activeFocus = false;
                    changed << afi;
                }
                afi = afi->parentItem();
            }
        }
    }

    if (item != contentItem && !(options & DontChangeSubFocusItem)) {
        QQuickItem *oldSubFocusItem = scopePrivate->subFocusItem;
        if (oldSubFocusItem) {
            QQuickItemPrivate::get(oldSubFocusItem)->focus = false;
            changed << oldSubFocusItem;
        }

        QQuickItemPrivate::get(item)->updateSubFocusItem(scope, true);
    }

    if (!(options & DontChangeFocusProperty)) {
        if (item != contentItem || windowHasFocus(q)) {
            itemPrivate->focus = true;
            changed << item;
        }
    }

    if (newActiveFocusItem && contentItem->hasFocus()) {
        activeFocusItem = newActiveFocusItem;

        QQuickItemPrivate::get(newActiveFocusItem)->activeFocus = true;
        changed << newActiveFocusItem;

        QQuickItem *afi = newActiveFocusItem->parentItem();
        while (afi && afi != scope) {
            if (afi->isFocusScope()) {
                QQuickItemPrivate::get(afi)->activeFocus = true;
                changed << afi;
            }
            afi = afi->parentItem();
        }
        updateFocusItemTransform();
        sendFocusIn = true;
    }

    // Now that all the state is changed, emit signals & events
    // We must do this last, as this process may result in further changes to focus.
    if (oldActiveFocusItem) {
        QFocusEvent event(QEvent::FocusOut, reason);
        QCoreApplication::sendEvent(oldActiveFocusItem, &event);
    }

    // Make sure that the FocusOut didn't result in another focus change.
    if (sendFocusIn && activeFocusItem == newActiveFocusItem) {
        QFocusEvent event(QEvent::FocusIn, reason);
        QCoreApplication::sendEvent(newActiveFocusItem, &event);
    }

    if (activeFocusItem != currentActiveFocusItem)
        emit q->focusObjectChanged(activeFocusItem);

    if (!changed.isEmpty())
        notifyFocusChangesRecur(changed.data(), changed.count() - 1);
}

void QQuickWindowPrivate::clearFocusInScope(QQuickItem *scope, QQuickItem *item, Qt::FocusReason reason, FocusOptions options)
{
    Q_Q(QQuickWindow);

    Q_ASSERT(item);
    Q_ASSERT(scope || item == contentItem);

    qCDebug(DBG_FOCUS) << "QQuickWindowPrivate::clearFocusInScope():";
    qCDebug(DBG_FOCUS) << "    scope:" << (QObject *)scope;
    qCDebug(DBG_FOCUS) << "    item:" << (QObject *)item;
    qCDebug(DBG_FOCUS) << "    activeFocusItem:" << (QObject *)activeFocusItem;

    QQuickItemPrivate *scopePrivate = 0;
    if (scope) {
        scopePrivate = QQuickItemPrivate::get(scope);
        if ( !scopePrivate->subFocusItem )
            return;//No focus, nothing to do.
    }

    QQuickItem *currentActiveFocusItem = activeFocusItem;
    QQuickItem *oldActiveFocusItem = 0;
    QQuickItem *newActiveFocusItem = 0;

    lastFocusReason = reason;

    QVarLengthArray<QQuickItem *, 20> changed;

    Q_ASSERT(item == contentItem || item == scopePrivate->subFocusItem);

    // Does this change the active focus?
    if (item == contentItem || scopePrivate->activeFocus) {
        oldActiveFocusItem = activeFocusItem;
        newActiveFocusItem = scope;

#ifndef QT_NO_IM
        QGuiApplication::inputMethod()->commit();
#endif

        activeFocusItem = 0;

        if (oldActiveFocusItem) {
            QQuickItem *afi = oldActiveFocusItem;
            while (afi && afi != scope) {
                if (QQuickItemPrivate::get(afi)->activeFocus) {
                    QQuickItemPrivate::get(afi)->activeFocus = false;
                    changed << afi;
                }
                afi = afi->parentItem();
            }
        }
    }

    if (item != contentItem && !(options & DontChangeSubFocusItem)) {
        QQuickItem *oldSubFocusItem = scopePrivate->subFocusItem;
        if (oldSubFocusItem && !(options & DontChangeFocusProperty)) {
            QQuickItemPrivate::get(oldSubFocusItem)->focus = false;
            changed << oldSubFocusItem;
        }

        QQuickItemPrivate::get(item)->updateSubFocusItem(scope, false);

    } else if (!(options & DontChangeFocusProperty)) {
        QQuickItemPrivate::get(item)->focus = false;
        changed << item;
    }

    if (newActiveFocusItem) {
        Q_ASSERT(newActiveFocusItem == scope);
        activeFocusItem = scope;
        updateFocusItemTransform();
    }

    // Now that all the state is changed, emit signals & events
    // We must do this last, as this process may result in further changes to
    // focus.
    if (oldActiveFocusItem) {
        QFocusEvent event(QEvent::FocusOut, reason);
        QCoreApplication::sendEvent(oldActiveFocusItem, &event);
    }

    // Make sure that the FocusOut didn't result in another focus change.
    if (newActiveFocusItem && activeFocusItem == newActiveFocusItem) {
        QFocusEvent event(QEvent::FocusIn, reason);
        QCoreApplication::sendEvent(newActiveFocusItem, &event);
    }

    if (activeFocusItem != currentActiveFocusItem)
        emit q->focusObjectChanged(activeFocusItem);

    if (!changed.isEmpty())
        notifyFocusChangesRecur(changed.data(), changed.count() - 1);
}

void QQuickWindowPrivate::clearFocusObject()
{
    if (activeFocusItem == contentItem)
        return;

    clearFocusInScope(contentItem, QQuickItemPrivate::get(contentItem)->subFocusItem, Qt::OtherFocusReason);
}

void QQuickWindowPrivate::notifyFocusChangesRecur(QQuickItem **items, int remaining)
{
    QPointer<QQuickItem> item(*items);

    if (remaining)
        notifyFocusChangesRecur(items + 1, remaining - 1);

    if (item) {
        QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

        if (itemPrivate->notifiedFocus != itemPrivate->focus) {
            itemPrivate->notifiedFocus = itemPrivate->focus;
            emit item->focusChanged(itemPrivate->focus);
        }

        if (item && itemPrivate->notifiedActiveFocus != itemPrivate->activeFocus) {
            itemPrivate->notifiedActiveFocus = itemPrivate->activeFocus;
            itemPrivate->itemChange(QQuickItem::ItemActiveFocusHasChanged, itemPrivate->activeFocus);
            emit item->activeFocusChanged(itemPrivate->activeFocus);
        }
    }
}

void QQuickWindowPrivate::dirtyItem(QQuickItem *)
{
    Q_Q(QQuickWindow);
    q->maybeUpdate();
}

void QQuickWindowPrivate::cleanup(QSGNode *n)
{
    Q_Q(QQuickWindow);

    Q_ASSERT(!cleanupNodeList.contains(n));
    cleanupNodeList.append(n);
    q->maybeUpdate();
}

/*!
    \qmltype Window
    \instantiates QQuickWindow
    \inqmlmodule QtQuick.Window
    \ingroup qtquick-visual
    \brief Creates a new top-level window

    The Window object creates a new top-level window for a Qt Quick scene. It automatically sets up the
    window for use with \c {QtQuick 2.x} graphical types.

    To use this type, you will need to import the module with the following line:
    \code
    import QtQuick.Window 2.2
    \endcode

    Omitting this import will allow you to have a QML environment without
    access to window system features.

    A Window can be declared inside an Item or inside another Window; in that
    case the inner Window will automatically become "transient for" the outer
    Window: that is, most platforms will show it centered upon the outer window
    by default, and there may be other platform-dependent behaviors, depending
    also on the \l flags. If the nested window is intended to be a dialog in
    your application, you should also set \l flags to Qt.Dialog, because some
    window managers will not provide the centering behavior without that flag.
    You can also declare multiple windows inside a top-level \l QtObject, in which
    case the windows will have no transient relationship.

    Alternatively you can set or bind \l x and \l y to position the Window
    explicitly on the screen.

    When the user attempts to close a window, the \l closing signal will be
    emitted. You can force the window to stay open (for example to prompt the
    user to save changes) by writing an \c onClosing handler and setting
    \c {close.accepted = false}.
*/
/*!
    \class QQuickWindow
    \since 5.0

    \inmodule QtQuick

    \brief The QQuickWindow class provides the window for displaying a graphical QML scene

    QQuickWindow provides the graphical scene management needed to interact with and display
    a scene of QQuickItems.

    A QQuickWindow always has a single invisible root item. To add items to this window,
    reparent the items to the root item or to an existing item in the scene.

    For easily displaying a scene from a QML file, see \l{QQuickView}.

    \section1 Rendering

    QQuickWindow uses a scene graph to represent what needs to be rendered.
    This scene graph is disconnected from the QML scene and
    potentially lives in another thread, depending on the platform
    implementation. Since the rendering scene graph lives
    independently from the QML scene, it can also be completely
    released without affecting the state of the QML scene.

    The sceneGraphInitialized() signal is emitted on the rendering
    thread before the QML scene is rendered to the screen for the
    first time. If the rendering scene graph has been released, the
    signal will be emitted again before the next frame is rendered.


    \section2 Integration with OpenGL

    When using the default OpenGL adaptation, it is possible to integrate
    OpenGL calls directly into the QQuickWindow using the same OpenGL
    context as the Qt Quick Scene Graph. This is done by connecting to the
    QQuickWindow::beforeRendering() or QQuickWindow::afterRendering()
    signal.

    \note When using QQuickWindow::beforeRendering(), make sure to
    disable clearing before rendering with
    QQuickWindow::setClearBeforeRendering().


    \section2 Exposure and Visibility

    When a QQuickWindow instance is deliberately hidden with hide() or
    setVisible(false), it will stop rendering and its scene graph and
    graphics context might be released. The sceneGraphInvalidated()
    signal will be emitted when this happens.

    \warning It is crucial that graphics operations and interaction with
    the scene graph happens exclusively on the rendering thread,
    primarily during the updatePaintNode() phase.

    \warning As signals related to rendering might be emitted from the
    rendering thread, connections should be made using
    Qt::DirectConnection.


    \section2 Resource Management

    QML will try to cache images and scene graph nodes to
    improve performance, but in some low-memory scenarios it might be
    required to aggressively release these resources. The
    releaseResources() can be used to force the clean up of certain
    resources. Calling releaseResources() may result in the entire
    scene graph and in the case of the OpenGL adaptation the associated
    context will be deleted. The sceneGraphInvalidated() signal will be
    emitted when this happens.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

    \section2 Context and surface formats

    While it is possible to specify a QSurfaceFormat for every QQuickWindow by
    calling the member function setFormat(), windows may also be created from
    QML by using the Window and ApplicationWindow elements. In this case there
    is no C++ code involved in the creation of the window instance, yet
    applications may still wish to set certain surface format values, for
    example to request a given OpenGL version or profile. Such applications can
    call the static function QSurfaceFormat::setDefaultFormat() at startup. The
    specified format will be used for all Quick windows created afterwards.

    \sa {Scene Graph - OpenGL Under QML}
*/

/*!
    Constructs a window for displaying a QML scene with parent window \a parent.
*/
QQuickWindow::QQuickWindow(QWindow *parent)
    : QQuickWindow(*new QQuickWindowPrivate, parent)
{
}



/*!
    \internal
*/
QQuickWindow::QQuickWindow(QQuickWindowPrivate &dd, QWindow *parent)
    : QWindow(dd, parent)
{
    Q_D(QQuickWindow);
    d->init(this);
}

/*!
    \internal
*/
QQuickWindow::QQuickWindow(QQuickRenderControl *control)
    : QWindow(*(new QQuickWindowPrivate), 0)
{
    Q_D(QQuickWindow);
    d->init(this, control);
}



/*!
    Destroys the window.
*/
QQuickWindow::~QQuickWindow()
{
    Q_D(QQuickWindow);

    if (d->renderControl) {
        QQuickRenderControlPrivate::get(d->renderControl)->windowDestroyed();
    } else if (d->windowManager) {
        d->windowManager->removeWindow(this);
        d->windowManager->windowDestroyed(this);
    }

    delete d->incubationController; d->incubationController = 0;
#ifndef QT_NO_DRAGANDDROP
    delete d->dragGrabber; d->dragGrabber = 0;
#endif
    delete d->contentItem; d->contentItem = 0;

    d->renderJobMutex.lock();
    qDeleteAll(d->beforeSynchronizingJobs);
    d->beforeSynchronizingJobs.clear();
    qDeleteAll(d->afterSynchronizingJobs);
    d->afterSynchronizingJobs.clear();
    qDeleteAll(d->beforeRenderingJobs);
    d->beforeRenderingJobs.clear();
    qDeleteAll(d->afterRenderingJobs);
    d->afterRenderingJobs.clear();
    qDeleteAll(d->afterSwapJobs);
    d->afterSwapJobs.clear();
    d->renderJobMutex.unlock();

    // It is important that the pixmap cache is cleaned up during shutdown.
    // Besides playing nice, this also solves a practical problem that
    // QQuickTextureFactory implementations in other libraries need
    // have their destructors loaded while they the library is still
    // loaded into memory.
    QQuickPixmap::purgeCache();
}

/*!
    This function tries to release redundant resources currently held by the QML scene.

    Calling this function might result in the scene graph and the OpenGL context used
    for rendering being released to release graphics memory. If this happens, the
    sceneGraphInvalidated() signal will be called, allowing users to clean up their
    own graphics resources. The setPersistentOpenGLContext() and setPersistentSceneGraph()
    functions can be used to prevent this from happening, if handling the cleanup is
    not feasible in the application, at the cost of higher memory usage.

    \sa sceneGraphInvalidated(), setPersistentOpenGLContext(), setPersistentSceneGraph()
 */

void QQuickWindow::releaseResources()
{
    Q_D(QQuickWindow);
    if (d->windowManager)
        d->windowManager->releaseResources(this);
    QQuickPixmap::purgeCache();
}



/*!
    Sets whether the OpenGL context should be preserved, and cannot be
    released until the last window is deleted, to \a persistent. The
    default value is true.

    The OpenGL context can be released to free up graphics resources
    when the window is obscured, hidden or not rendering. When this
    happens is implementation specific.

    The QOpenGLContext::aboutToBeDestroyed() signal is emitted from
    the QQuickWindow::openglContext() when the OpenGL context is about
    to be released.  The QQuickWindow::sceneGraphInitialized() signal
    is emitted when a new OpenGL context is created for this
    window. Make a Qt::DirectConnection to these signals to be
    notified.

    The OpenGL context is still released when the last QQuickWindow is
    deleted.

    \note This only has an effect when using the default OpenGL scene
    graph adaptation.

    \sa setPersistentSceneGraph(),
    QOpenGLContext::aboutToBeDestroyed(), sceneGraphInitialized()
 */

void QQuickWindow::setPersistentOpenGLContext(bool persistent)
{
    Q_D(QQuickWindow);
    d->persistentGLContext = persistent;
}



/*!
    Returns whether the OpenGL context can be released during the
    lifetime of the QQuickWindow.

    \note This is a hint. When and how this happens is implementation
    specific.  It also only has an effect when using the default OpenGL
    scene graph adaptation
 */

bool QQuickWindow::isPersistentOpenGLContext() const
{
    Q_D(const QQuickWindow);
    return d->persistentGLContext;
}



/*!
    Sets whether the scene graph nodes and resources can be released
    to \a persistent.  The default value is true.

    The scene graph nodes and resources can be released to free up
    graphics resources when the window is obscured, hidden or not
    rendering. When this happens is implementation specific.

    The QQuickWindow::sceneGraphInvalidated() signal is emitted when
    cleanup occurs. The QQuickWindow::sceneGraphInitialized() signal
    is emitted when a new scene graph is recreated for this
    window. Make a Qt::DirectConnection to these signals to be
    notified.

    The scene graph nodes and resources are still released when the
    last QQuickWindow is deleted.

    \sa setPersistentOpenGLContext(),
    sceneGraphInvalidated(), sceneGraphInitialized()
 */

void QQuickWindow::setPersistentSceneGraph(bool persistent)
{
    Q_D(QQuickWindow);
    d->persistentSceneGraph = persistent;
}



/*!
    Returns whether the scene graph nodes and resources can be
    released during the lifetime of this QQuickWindow.

    \note This is a hint. When and how this happens is implementation
    specific.
 */

bool QQuickWindow::isPersistentSceneGraph() const
{
    Q_D(const QQuickWindow);
    return d->persistentSceneGraph;
}




/*!
    \qmlattachedproperty Item Window::contentItem
    \since 5.4

    This attached property holds the invisible root item of the scene or
    \c null if the item is not in a window. The Window attached property
    can be attached to any Item.
*/

/*!
    \property QQuickWindow::contentItem
    \brief The invisible root item of the scene.

  A QQuickWindow always has a single invisible root item containing all of its content.
  To add items to this window, reparent the items to the contentItem or to an existing
  item in the scene.
*/
QQuickItem *QQuickWindow::contentItem() const
{
    Q_D(const QQuickWindow);

    return d->contentItem;
}

/*!
    \property QQuickWindow::activeFocusItem

    \brief The item which currently has active focus or \c null if there is
    no item with active focus.
*/
QQuickItem *QQuickWindow::activeFocusItem() const
{
    Q_D(const QQuickWindow);

    return d->activeFocusItem;
}

/*!
  \internal
  \reimp
*/
QObject *QQuickWindow::focusObject() const
{
    Q_D(const QQuickWindow);

    if (d->activeFocusItem)
        return d->activeFocusItem;
    return const_cast<QQuickWindow*>(this);
}


/*!
  Returns the item which currently has the mouse grab.
*/
QQuickItem *QQuickWindow::mouseGrabberItem() const
{
    Q_D(const QQuickWindow);

    if (d->touchMouseId != -1 && d->touchMouseDevice) {
        QQuickPointerEvent *event = d->touchMouseDevice->pointerEvent();
        auto point = event->pointById(d->touchMouseId);
        Q_ASSERT(point);
        return point->grabber();
    }

    QQuickPointerEvent *event = QQuickPointerDevice::genericMouseDevice()->pointerEvent();
    Q_ASSERT(event->pointCount());
    return event->point(0)->grabber();
}


bool QQuickWindowPrivate::clearHover(ulong timestamp)
{
    Q_Q(QQuickWindow);
    if (hoverItems.isEmpty())
        return false;

    QPointF pos = q->mapFromGlobal(QGuiApplicationPrivate::lastCursorPosition.toPoint());

    bool accepted = false;
    for (QQuickItem* item : qAsConst(hoverItems))
        accepted = sendHoverEvent(QEvent::HoverLeave, item, pos, pos, QGuiApplication::keyboardModifiers(), timestamp, true) || accepted;
    hoverItems.clear();
    return accepted;
}

/*! \reimp */
bool QQuickWindow::event(QEvent *e)
{
    Q_D(QQuickWindow);

    switch (e->type()) {

    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
        QTouchEvent *touch = static_cast<QTouchEvent*>(e);
        d->handleTouchEvent(touch);
        if (Q_LIKELY(QCoreApplication::testAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents))) {
            // we consume all touch events ourselves to avoid duplicate
            // mouse delivery by QtGui mouse synthesis
            e->accept();
        }
        return true;
    }
        break;
    case QEvent::TouchCancel:
        // return in order to avoid the QWindow::event below
        return d->deliverTouchCancelEvent(static_cast<QTouchEvent*>(e));
        break;
    case QEvent::Enter: {
        QEnterEvent *enter = static_cast<QEnterEvent*>(e);
        bool accepted = enter->isAccepted();
        bool delivered = d->deliverHoverEvent(d->contentItem, enter->windowPos(), d->lastMousePosition,
            QGuiApplication::keyboardModifiers(), 0L, accepted);
        enter->setAccepted(accepted);
        return delivered;
    }
        break;
    case QEvent::Leave:
        d->clearHover();
        d->lastMousePosition = QPointF();
        break;
#ifndef QT_NO_DRAGANDDROP
    case QEvent::DragEnter:
    case QEvent::DragLeave:
    case QEvent::DragMove:
    case QEvent::Drop:
        d->deliverDragEvent(d->dragGrabber, e);
        break;
#endif
    case QEvent::WindowDeactivate:
        contentItem()->windowDeactivateEvent();
        break;
    case QEvent::Close: {
        // TOOD Qt 6 (binary incompatible)
        // closeEvent(static_cast<QCloseEvent *>(e));
        QQuickCloseEvent qev;
        qev.setAccepted(e->isAccepted());
        emit closing(&qev);
        e->setAccepted(qev.isAccepted());
        } break;
    case QEvent::FocusAboutToChange:
#ifndef QT_NO_IM
        if (d->activeFocusItem)
            qGuiApp->inputMethod()->commit();
#endif
        if (mouseGrabberItem())
            mouseGrabberItem()->ungrabMouse();
        break;
    case QEvent::UpdateRequest: {
        if (d->windowManager)
            d->windowManager->handleUpdateRequest(this);
        break;
    }
#ifndef QT_NO_GESTURES
    case QEvent::NativeGesture:
        d->deliverNativeGestureEvent(d->contentItem, static_cast<QNativeGestureEvent*>(e));
        break;
#endif
    case QEvent::ShortcutOverride:
        if (d->activeFocusItem)
            QCoreApplication::sendEvent(d->activeFocusItem, e);
        return true;
    default:
        break;
    }

    if (e->type() == QEvent::Type(QQuickWindowPrivate::FullUpdateRequest))
        update();

    return QWindow::event(e);
}

/*! \reimp */
void QQuickWindow::keyPressEvent(QKeyEvent *e)
{
    Q_D(QQuickWindow);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Key, QQuickProfiler::InputKeyPress, e->key(),
                          e->modifiers());
    d->deliverKeyEvent(e);
}

/*! \reimp */
void QQuickWindow::keyReleaseEvent(QKeyEvent *e)
{
    Q_D(QQuickWindow);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Key, QQuickProfiler::InputKeyRelease, e->key(),
                          e->modifiers());
    d->deliverKeyEvent(e);
}

void QQuickWindowPrivate::deliverKeyEvent(QKeyEvent *e)
{
    Q_Q(QQuickWindow);

    if (activeFocusItem)
        q->sendEvent(activeFocusItem, e);
}

QMouseEvent *QQuickWindowPrivate::cloneMouseEvent(QMouseEvent *event, QPointF *transformedLocalPos)
{
    int caps = QGuiApplicationPrivate::mouseEventCaps(event);
    QVector2D velocity = QGuiApplicationPrivate::mouseEventVelocity(event);
    QMouseEvent *me = new QMouseEvent(event->type(),
                                      transformedLocalPos ? *transformedLocalPos : event->localPos(),
                                      event->windowPos(), event->screenPos(),
                                      event->button(), event->buttons(), event->modifiers());
    QGuiApplicationPrivate::setMouseEventCapsAndVelocity(me, caps, velocity);
    QGuiApplicationPrivate::setMouseEventSource(me, QGuiApplicationPrivate::mouseEventSource(event));
    me->setTimestamp(event->timestamp());
    return me;
}

void QQuickWindowPrivate::deliverMouseEvent(QQuickPointerMouseEvent *pointerEvent)
{
    Q_Q(QQuickWindow);
    auto point = pointerEvent->point(0);
    lastMousePosition = point->scenePos();
    QQuickItem *grabber = point->grabber();
    if (grabber) {
        // if the update consists of changing button state, then don't accept it
        // unless the button is one in which the item is interested
        if (pointerEvent->button() != Qt::NoButton
                && grabber->acceptedMouseButtons()
                && !(grabber->acceptedMouseButtons() & pointerEvent->button())) {
            pointerEvent->setAccepted(false);
            return;
        }

        // send update
        QPointF localPos = grabber->mapFromScene(lastMousePosition);
        auto me = pointerEvent->asMouseEvent(localPos);
        me->accept();
        q->sendEvent(grabber, me);
        point->setAccepted(me->isAccepted());

        // release event, make sure to ungrab if there still is a grabber
        if (me->type() == QEvent::MouseButtonRelease && !me->buttons() && q->mouseGrabberItem())
            q->mouseGrabberItem()->ungrabMouse();
    } else {
        // send initial press
        bool delivered = false;
        if (pointerEvent->isPressEvent()) {
            QSet<QQuickItem*> hasFiltered;
            delivered = deliverPressEvent(pointerEvent, &hasFiltered);
        }

        if (!delivered)
            // make sure not to accept unhandled events
            pointerEvent->setAccepted(false);
    }
}

bool QQuickWindowPrivate::sendHoverEvent(QEvent::Type type, QQuickItem *item,
                                      const QPointF &scenePos, const QPointF &lastScenePos,
                                      Qt::KeyboardModifiers modifiers, ulong timestamp,
                                      bool accepted)
{
    const QTransform transform = QQuickItemPrivate::get(item)->windowToItemTransform();

    //create copy of event
    QHoverEvent hoverEvent(type, transform.map(scenePos), transform.map(lastScenePos), modifiers);
    hoverEvent.setTimestamp(timestamp);
    hoverEvent.setAccepted(accepted);

    QSet<QQuickItem *> hasFiltered;
    if (sendFilteredMouseEvent(item->parentItem(), item, &hoverEvent, &hasFiltered)) {
        return true;
    }

    QCoreApplication::sendEvent(item, &hoverEvent);

    return hoverEvent.isAccepted();
}

bool QQuickWindowPrivate::deliverHoverEvent(QQuickItem *item, const QPointF &scenePos, const QPointF &lastScenePos,
                                         Qt::KeyboardModifiers modifiers, ulong timestamp, bool &accepted)
{
    Q_Q(QQuickWindow);
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

    if (itemPrivate->flags & QQuickItem::ItemClipsChildrenToShape) {
        QPointF p = item->mapFromScene(scenePos);
        if (!item->contains(p))
            return false;
    }

    qCDebug(DBG_HOVER_TRACE) << q << item << scenePos << lastScenePos << "subtreeHoverEnabled" << itemPrivate->subtreeHoverEnabled;
    if (itemPrivate->subtreeHoverEnabled) {
        QList<QQuickItem *> children = itemPrivate->paintOrderChildItems();
        for (int ii = children.count() - 1; ii >= 0; --ii) {
            QQuickItem *child = children.at(ii);
            if (!child->isVisible() || !child->isEnabled() || QQuickItemPrivate::get(child)->culled)
                continue;
            if (deliverHoverEvent(child, scenePos, lastScenePos, modifiers, timestamp, accepted))
                return true;
        }
    }

    if (itemPrivate->hoverEnabled) {
        QPointF p = item->mapFromScene(scenePos);
        if (item->contains(p)) {
            if (!hoverItems.isEmpty() && hoverItems.at(0) == item) {
                //move
                accepted = sendHoverEvent(QEvent::HoverMove, item, scenePos, lastScenePos, modifiers, timestamp, accepted);
            } else {
                QList<QQuickItem *> itemsToHover;
                QQuickItem* parent = item;
                itemsToHover << item;
                while ((parent = parent->parentItem()))
                    itemsToHover << parent;

                // Leaving from previous hovered items until we reach the item or one of its ancestors.
                while (!hoverItems.isEmpty() && !itemsToHover.contains(hoverItems.at(0))) {
                    QQuickItem *hoverLeaveItem = hoverItems.takeFirst();
                    sendHoverEvent(QEvent::HoverLeave, hoverLeaveItem, scenePos, lastScenePos, modifiers, timestamp, accepted);
                }

                if (!hoverItems.isEmpty() && hoverItems.at(0) == item) {//Not entering a new Item
                    // ### Shouldn't we send moves for the parent items as well?
                    accepted = sendHoverEvent(QEvent::HoverMove, item, scenePos, lastScenePos, modifiers, timestamp, accepted);
                } else {
                    // Enter items that are not entered yet.
                    int startIdx = -1;
                    if (!hoverItems.isEmpty())
                        startIdx = itemsToHover.indexOf(hoverItems.at(0)) - 1;
                    if (startIdx == -1)
                        startIdx = itemsToHover.count() - 1;

                    for (int i = startIdx; i >= 0; i--) {
                        QQuickItem *itemToHover = itemsToHover.at(i);
                        QQuickItemPrivate *itemToHoverPrivate = QQuickItemPrivate::get(itemToHover);
                        // The item may be about to be deleted or reparented to another window
                        // due to another hover event delivered in this function. If that is the
                        // case, sending a hover event here will cause a crash or other bad
                        // behavior when the leave event is generated. Checking
                        // itemToHoverPrivate->window here prevents that case.
                        if (itemToHoverPrivate->window == q && itemToHoverPrivate->hoverEnabled) {
                            hoverItems.prepend(itemToHover);
                            sendHoverEvent(QEvent::HoverEnter, itemToHover, scenePos, lastScenePos, modifiers, timestamp, accepted);
                        }
                    }
                }
            }
            return true;
        }
    }

    return false;
}

#ifndef QT_NO_WHEELEVENT
bool QQuickWindowPrivate::deliverWheelEvent(QQuickItem *item, QWheelEvent *event)
{
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

    if (itemPrivate->flags & QQuickItem::ItemClipsChildrenToShape) {
        QPointF p = item->mapFromScene(event->posF());
        if (!item->contains(p))
            return false;
    }

    QList<QQuickItem *> children = itemPrivate->paintOrderChildItems();
    for (int ii = children.count() - 1; ii >= 0; --ii) {
        QQuickItem *child = children.at(ii);
        if (!child->isVisible() || !child->isEnabled() || QQuickItemPrivate::get(child)->culled)
            continue;
        if (deliverWheelEvent(child, event))
            return true;
    }

    QPointF p = item->mapFromScene(event->posF());

    if (item->contains(p)) {
        QWheelEvent wheel(p, p, event->pixelDelta(), event->angleDelta(), event->delta(),
                          event->orientation(), event->buttons(), event->modifiers(), event->phase(), event->source(), event->inverted());
        wheel.setTimestamp(event->timestamp());
        wheel.accept();
        QCoreApplication::sendEvent(item, &wheel);
        if (wheel.isAccepted()) {
            event->accept();
            return true;
        }
    }

    return false;
}

/*! \reimp */
void QQuickWindow::wheelEvent(QWheelEvent *event)
{
    Q_D(QQuickWindow);
    Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMouseWheel,
                          event->angleDelta().x(), event->angleDelta().y());

    qCDebug(DBG_MOUSE) << "QQuickWindow::wheelEvent()" << event->pixelDelta() << event->angleDelta() << event->phase();

    //if the actual wheel event was accepted, accept the compatibility wheel event and return early
    if (d->lastWheelEventAccepted && event->angleDelta().isNull() && event->phase() == Qt::ScrollUpdate)
        return;

    event->ignore();
    d->deliverWheelEvent(d->contentItem, event);
    d->lastWheelEventAccepted = event->isAccepted();
}
#endif // QT_NO_WHEELEVENT

#ifndef QT_NO_GESTURES
bool QQuickWindowPrivate::deliverNativeGestureEvent(QQuickItem *item, QNativeGestureEvent *event)
{
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

    if ((itemPrivate->flags & QQuickItem::ItemClipsChildrenToShape) && !item->contains(event->localPos()))
        return false;

    QList<QQuickItem *> children = itemPrivate->paintOrderChildItems();
    for (int ii = children.count() - 1; ii >= 0; --ii) {
        QQuickItem *child = children.at(ii);
        if (!child->isVisible() || !child->isEnabled() || QQuickItemPrivate::get(child)->culled)
            continue;
        if (deliverNativeGestureEvent(child, event))
            return true;
    }

    QPointF p = item->mapFromScene(event->localPos());

    if (item->contains(p)) {
        QNativeGestureEvent copy(event->gestureType(), p, event->windowPos(), event->screenPos(),
                                 event->value(), 0L, 0L); // TODO can't copy things I can't access
        event->accept();
        item->event(&copy);
        if (copy.isAccepted()) {
            event->accept();
            return true;
        }
    }

    return false;
}
#endif // QT_NO_GESTURES

bool QQuickWindowPrivate::deliverTouchCancelEvent(QTouchEvent *event)
{
    qCDebug(DBG_TOUCH) << event;
    Q_Q(QQuickWindow);

    // A TouchCancel event will typically not contain any points.
    // Deliver it to all items that have active touches.
    QQuickPointerEvent *pointerEvent = QQuickPointerDevice::touchDevice(event->device())->pointerEvent();
    QVector<QQuickItem *> grabbers = pointerEvent->grabbers();

    for (QQuickItem *grabber: qAsConst(grabbers)) {
        q->sendEvent(grabber, event);
    }
    touchMouseId = -1;
    touchMouseDevice = nullptr;
    if (q->mouseGrabberItem())
        q->mouseGrabberItem()->ungrabMouse();

    // The next touch event can only be a TouchBegin so clean up.
    pointerEvent->clearGrabbers();
    return true;
}

void QQuickWindowPrivate::deliverDelayedTouchEvent()
{
    // Deliver and delete delayedTouch.
    // Set delayedTouch to 0 before delivery to avoid redelivery in case of
    // event loop recursions (e.g if it the touch starts a dnd session).
    QScopedPointer<QTouchEvent> e(delayedTouch.take());
    deliverPointerEvent(pointerEventInstance(e.data()));
}

static bool qquickwindow_no_touch_compression = qEnvironmentVariableIsSet("QML_NO_TOUCH_COMPRESSION");

bool QQuickWindowPrivate::compressTouchEvent(QTouchEvent *event)
{
    Q_Q(QQuickWindow);
    Qt::TouchPointStates states = event->touchPointStates();
    if (((states & (Qt::TouchPointMoved | Qt::TouchPointStationary)) == 0)
        || ((states & (Qt::TouchPointPressed | Qt::TouchPointReleased)) != 0)) {
        // we can only compress something that isn't a press or release
        return false;
    }

    if (!delayedTouch) {
        delayedTouch.reset(new QTouchEvent(event->type(), event->device(), event->modifiers(), event->touchPointStates(), event->touchPoints()));
        delayedTouch->setTimestamp(event->timestamp());
        if (renderControl)
            QQuickRenderControlPrivate::get(renderControl)->maybeUpdate();
        else if (windowManager)
            windowManager->maybeUpdate(q);
        return true;
    }

    // check if this looks like the last touch event
    if (delayedTouch->type() == event->type() &&
            delayedTouch->device() == event->device() &&
            delayedTouch->modifiers() == event->modifiers() &&
            delayedTouch->touchPoints().count() == event->touchPoints().count())
    {
        // possible match.. is it really the same?
        bool mismatch = false;

        QList<QTouchEvent::TouchPoint> tpts = event->touchPoints();
        Qt::TouchPointStates states;
        for (int i = 0; i < event->touchPoints().count(); ++i) {
            const QTouchEvent::TouchPoint &tp = tpts.at(i);
            const QTouchEvent::TouchPoint &tpDelayed = delayedTouch->touchPoints().at(i);
            if (tp.id() != tpDelayed.id()) {
                mismatch = true;
                break;
            }

            if (tpDelayed.state() == Qt::TouchPointMoved && tp.state() == Qt::TouchPointStationary)
                tpts[i].setState(Qt::TouchPointMoved);
            tpts[i].setLastPos(tpDelayed.lastPos());
            tpts[i].setLastScenePos(tpDelayed.lastScenePos());
            tpts[i].setLastScreenPos(tpDelayed.lastScreenPos());
            tpts[i].setLastNormalizedPos(tpDelayed.lastNormalizedPos());

            states |= tpts.at(i).state();
        }

        // matching touch event? then merge the new event into the old one
        if (!mismatch) {
            delayedTouch->setTouchPoints(tpts);
            delayedTouch->setTimestamp(event->timestamp());
            return true;
        }
    }

    // merging wasn't possible, so deliver the delayed event first, and then delay this one
    deliverDelayedTouchEvent();
    delayedTouch.reset(new QTouchEvent(event->type(), event->device(), event->modifiers(), event->touchPointStates(), event->touchPoints()));
    delayedTouch->setTimestamp(event->timestamp());
    return true;
}

// entry point for touch event delivery:
// - translate the event to window coordinates
// - compress the event instead of delivering it if applicable
// - call deliverTouchPoints to actually dispatch the points
void QQuickWindowPrivate::handleTouchEvent(QTouchEvent *event)
{
    translateTouchEvent(event);
    if (event->touchPoints().size())
        lastMousePosition = event->touchPoints().at(0).pos();

    qCDebug(DBG_TOUCH) << event;

    if (qquickwindow_no_touch_compression || pointerEventRecursionGuard) {
        deliverPointerEvent(pointerEventInstance(event));
        return;
    }

    if (!compressTouchEvent(event)) {
        if (delayedTouch)
            deliverDelayedTouchEvent();
        deliverPointerEvent(pointerEventInstance(event));
    }
}

/*! \reimp */
void QQuickWindow::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickWindow);
    d->handleMouseEvent(event);
}
/*! \reimp */
void QQuickWindow::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickWindow);
    d->handleMouseEvent(event);
}
/*! \reimp */
void QQuickWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickWindow);
    d->handleMouseEvent(event);
}
/*! \reimp */
void QQuickWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickWindow);
    d->handleMouseEvent(event);
}

void QQuickWindowPrivate::handleMouseEvent(QMouseEvent *event)
{
    Q_Q(QQuickWindow);

    if (event->source() == Qt::MouseEventSynthesizedBySystem) {
        event->accept();
        return;
    }
    qCDebug(DBG_MOUSE) << "QQuickWindow::handleMouseEvent()" << event->type() << event->localPos() << event->button() << event->buttons();

    switch (event->type()) {
    case QEvent::MouseButtonPress:
        Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMousePress, event->button(),
                              event->buttons());
        deliverPointerEvent(pointerEventInstance(event));
        break;
    case QEvent::MouseButtonRelease:
        Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMouseRelease, event->button(),
                              event->buttons());
        deliverPointerEvent(pointerEventInstance(event));
        break;
    case QEvent::MouseButtonDblClick:
        Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMouseDoubleClick,
                              event->button(), event->buttons());
        deliverPointerEvent(pointerEventInstance(event));
        break;
    case QEvent::MouseMove:
        Q_QUICK_INPUT_PROFILE(QQuickProfiler::Mouse, QQuickProfiler::InputMouseMove,
                              event->localPos().x(), event->localPos().y());

        qCDebug(DBG_HOVER_TRACE) << this;

    #ifndef QT_NO_CURSOR
        updateCursor(event->windowPos());
    #endif

        if (!q->mouseGrabberItem()) {
            QPointF last = lastMousePosition.isNull() ? event->windowPos() : lastMousePosition;
            lastMousePosition = event->windowPos();

            bool accepted = event->isAccepted();
            bool delivered = deliverHoverEvent(contentItem, event->windowPos(), last, event->modifiers(), event->timestamp(), accepted);
            if (!delivered) {
                //take care of any exits
                accepted = clearHover(event->timestamp());
            }
            event->setAccepted(accepted);
            return;
        }
        deliverPointerEvent(pointerEventInstance(event));
        break;
    default:
        Q_ASSERT(false);
        break;
    }
}

void QQuickWindowPrivate::flushFrameSynchronousEvents()
{
    Q_Q(QQuickWindow);

    if (delayedTouch) {
        deliverDelayedTouchEvent();

        // Touch events which constantly start animations (such as a behavior tracking
        // the mouse point) need animations to start.
        QQmlAnimationTimer *ut = QQmlAnimationTimer::instance();
        if (ut && ut->hasStartAnimationPending())
            ut->startAnimations();
    }

    // Once per frame, send a synthetic hover, in case items have changed position.
    // For instance, during animation (including the case of a ListView
    // whose delegates contain MouseAreas), a MouseArea needs to know
    // whether it has moved into a position where it is now under the cursor.
    if (!q->mouseGrabberItem() && !lastMousePosition.isNull()) {
        bool accepted = false;
        bool delivered = deliverHoverEvent(contentItem, lastMousePosition, lastMousePosition, QGuiApplication::keyboardModifiers(), 0, accepted);
        if (!delivered)
            clearHover(); // take care of any exits
    }
}

/*!
    \internal
    Returns a QQuickPointerEvent instance suitable for wrapping and delivering \a event.

    There is a unique instance per QQuickPointerDevice, which is determined
    from \a event's device.
*/
QQuickPointerEvent *QQuickWindowPrivate::pointerEventInstance(QEvent *event)
{
    QQuickPointerDevice *dev = nullptr;
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
        dev = QQuickPointerDevice::genericMouseDevice();
        break;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        dev = QQuickPointerDevice::touchDevice(static_cast<QTouchEvent *>(event)->device());
        break;
    // TODO tablet event types
    default:
        break;
    }
    Q_ASSERT(dev);
    return dev->pointerEvent()->reset(event);
}

void QQuickWindowPrivate::deliverPointerEvent(QQuickPointerEvent *event)
{
    // If users spin the eventloop as a result of event delivery, we disable
    // event compression and send events directly. This is because we consider
    // the usecase a bit evil, but we at least don't want to lose events.
    ++pointerEventRecursionGuard;

    if (event->asPointerMouseEvent()) {
        deliverMouseEvent(event->asPointerMouseEvent());
    } else if (event->asPointerTouchEvent()) {
        deliverTouchEvent(event->asPointerTouchEvent());
    } else {
        Q_ASSERT(false);
    }

    event->reset(nullptr);

    --pointerEventRecursionGuard;
}

// check if item or any of its child items contain the point
// FIXME: should this be iterative instead of recursive?
QVector<QQuickItem *> QQuickWindowPrivate::pointerTargets(QQuickItem *item, const QPointF &scenePos, bool checkMouseButtons) const
{
    QVector<QQuickItem *> targets;
    auto itemPrivate = QQuickItemPrivate::get(item);
    QPointF itemPos = item->mapFromScene(scenePos);
    // if the item clips, we can potentially return early
    if (itemPrivate->flags & QQuickItem::ItemClipsChildrenToShape) {
        if (!item->contains(itemPos))
            return targets;
    }

    // recurse for children
    QList<QQuickItem *> children = itemPrivate->paintOrderChildItems();
    for (int ii = children.count() - 1; ii >= 0; --ii) {
        QQuickItem *child = children.at(ii);
        auto childPrivate = QQuickItemPrivate::get(child);
        if (!child->isVisible() || !child->isEnabled() || childPrivate->culled)
            continue;
        targets << pointerTargets(child, scenePos, false);
    }

    if (item->contains(itemPos) && (!checkMouseButtons || itemPrivate->acceptedMouseButtons())) {
        // add this item last - children take precedence
        targets << item;
    }
    return targets;
}

// return the joined lists
// list1 has priority, common items come last
QVector<QQuickItem *> QQuickWindowPrivate::mergePointerTargets(const QVector<QQuickItem *> &list1, const QVector<QQuickItem *> &list2) const
{
    QVector<QQuickItem *> targets = list1;
    // start at the end of list2
    // if item not in list, append it
    // if item found, move to next one, inserting before the last found one
    int insertPosition = targets.length();
    for (int i = list2.length() - 1; i >= 0; --i) {
        int newInsertPosition = targets.lastIndexOf(list2.at(i), insertPosition);
        if (newInsertPosition >= 0) {
            Q_ASSERT(newInsertPosition <= insertPosition);
            insertPosition = newInsertPosition;
        }
        // check for duplicates, only insert if the item isn't there already
        if (insertPosition == targets.size() || list2.at(i) != targets.at(insertPosition))
            targets.insert(insertPosition, list2.at(i));
    }
    return targets;
}

void QQuickWindowPrivate::deliverTouchEvent(QQuickPointerTouchEvent *event)
{
    qCDebug(DBG_TOUCH) << " - delivering" << event->asTouchEvent();

    QSet<QQuickItem *> hasFiltered;
    if (event->isPressEvent())
        deliverPressEvent(event, &hasFiltered);
    if (!event->allPointsAccepted())
        deliverUpdatedTouchPoints(event, &hasFiltered);

    // Remove released points from itemForTouchPointId
    bool allReleased = true;
    int pointCount = event->pointCount();
    for (int i = 0; i < pointCount; ++i) {
        QQuickEventPoint *point = event->point(i);
        if (point->state() == QQuickEventPoint::Released) {
            int id = point->pointId();
            qCDebug(DBG_TOUCH_TARGET) << "TP" << id << "released";
            point->setGrabber(nullptr);
            if (id == touchMouseId) {
                touchMouseId = -1;
                touchMouseDevice = nullptr;
            }
        } else {
            allReleased = false;
        }
    }

    if (allReleased && !event->grabbers().isEmpty()) {
        qWarning() << "No release received for some grabbers" << event->grabbers();
        event->clearGrabbers();
    }
}

// Deliver touch points to existing grabbers
bool QQuickWindowPrivate::deliverUpdatedTouchPoints(QQuickPointerTouchEvent *event, QSet<QQuickItem *> *hasFiltered)
{
    const auto grabbers = event->grabbers();
    for (auto grabber : grabbers)
        deliverMatchingPointsToItem(grabber, event, hasFiltered);

    return false;
}

// Deliver newly pressed touch points
bool QQuickWindowPrivate::deliverPressEvent(QQuickPointerEvent *event, QSet<QQuickItem *> *hasFiltered)
{
    const QVector<QPointF> points = event->unacceptedPressedPointScenePositions();
    QVector<QQuickItem *> targetItems;
    for (QPointF point: points) {
        QVector<QQuickItem *> targetItemsForPoint = pointerTargets(contentItem, point, false);
        if (targetItems.count()) {
            targetItems = mergePointerTargets(targetItems, targetItemsForPoint);
        } else {
            targetItems = targetItemsForPoint;
        }
    }

    for (QQuickItem *item: targetItems) {
        deliverMatchingPointsToItem(item, event, hasFiltered);
        if (event->allPointsAccepted())
            break;
    }

    return event->allPointsAccepted();
}

bool QQuickWindowPrivate::deliverMatchingPointsToItem(QQuickItem *item, QQuickPointerEvent *pointerEvent, QSet<QQuickItem *> *hasFiltered)
{
    Q_Q(QQuickWindow);

    // TODO: unite this mouse point delivery with the synthetic mouse event below
    if (auto event = pointerEvent->asPointerMouseEvent()) {
        if (item->acceptedMouseButtons() & event->button()) {
            auto point = event->point(0);
            if (point->isAccepted())
                return false;

            // The only reason to already have a mouse grabber here is
            // synthetic events - flickable sends one when setPressDelay is used.
            auto oldMouseGrabber = q->mouseGrabberItem();
            QPointF localPos = item->mapFromScene(point->scenePos());
            Q_ASSERT(item->contains(localPos)); // transform is checked already
            QMouseEvent *me = event->asMouseEvent(localPos);
            me->accept();
            q->sendEvent(item, me);
            if (me->isAccepted()) {
                auto mouseGrabber = q->mouseGrabberItem();
                if (mouseGrabber && mouseGrabber != item && mouseGrabber != oldMouseGrabber) {
                    item->mouseUngrabEvent();
                } else {
                    item->grabMouse();
                }
                point->setAccepted(true);
            }
            return me->isAccepted();
        }
        return false;
    }

    QQuickPointerTouchEvent *event = pointerEvent->asPointerTouchEvent();
    if (!event)
        return false;

    QScopedPointer<QTouchEvent> touchEvent(event->touchEventForItem(item));
    if (!touchEvent)
        return false;

    qCDebug(DBG_TOUCH) << " - considering delivering " << touchEvent.data() << " to " << item;
    bool eventAccepted = false;

    // First check whether the parent wants to be a filter,
    // and if the parent accepts the event we are done.
    if (sendFilteredTouchEvent(item->parentItem(), item, event, hasFiltered)) {
        // If the touch was accepted (regardless by whom or in what form),
        // update acceptedNewPoints
        qCDebug(DBG_TOUCH) << " - can't. intercepted " << touchEvent.data() << " to " << item->parentItem() << " instead of " << item;
        for (auto point: qAsConst(touchEvent->touchPoints())) {
            event->pointById(point.id())->setAccepted();
        }
        return true;
    }

    // Deliver the touch event to the given item
    qCDebug(DBG_TOUCH) << " - actually delivering " << touchEvent.data() << " to " << item;
    QCoreApplication::sendEvent(item, touchEvent.data());
    eventAccepted = touchEvent->isAccepted();

    // If the touch event wasn't accepted, synthesize a mouse event and see if the item wants it.
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
    if (!eventAccepted && (itemPrivate->acceptedMouseButtons() & Qt::LeftButton)) {
        //  send mouse event
        if (deliverTouchAsMouse(item, event))
            eventAccepted = true;
    }

    if (eventAccepted) {
        // If the touch was accepted (regardless by whom or in what form),
        // update accepted new points.
        for (auto point: qAsConst(touchEvent->touchPoints())) {
            auto pointerEventPoint = event->pointById(point.id());
            pointerEventPoint->setAccepted();
            if (point.state() == Qt::TouchPointPressed)
                pointerEventPoint->setGrabber(item);
        }
    } else {
        // But if the event was not accepted then we know this item
        // will not be interested in further updates for those touchpoint IDs either.
        for (auto point: qAsConst(touchEvent->touchPoints())) {
            if (point.state() == Qt::TouchPointPressed) {
                if (event->pointById(point.id())->grabber() == item) {
                    qCDebug(DBG_TOUCH_TARGET) << "TP" << point.id() << "disassociated";
                    event->pointById(point.id())->setGrabber(nullptr);
                }
            }
        }
    }

    return eventAccepted;
}

#ifndef QT_NO_DRAGANDDROP
void QQuickWindowPrivate::deliverDragEvent(QQuickDragGrabber *grabber, QEvent *event)
{
    grabber->resetTarget();
    QQuickDragGrabber::iterator grabItem = grabber->begin();
    if (grabItem != grabber->end()) {
        Q_ASSERT(event->type() != QEvent::DragEnter);
        if (event->type() == QEvent::Drop) {
            QDropEvent *e = static_cast<QDropEvent *>(event);
            for (e->setAccepted(false); !e->isAccepted() && grabItem != grabber->end(); grabItem = grabber->release(grabItem)) {
                QPointF p = (**grabItem)->mapFromScene(e->pos());
                QDropEvent translatedEvent(
                        p.toPoint(),
                        e->possibleActions(),
                        e->mimeData(),
                        e->mouseButtons(),
                        e->keyboardModifiers());
                QQuickDropEventEx::copyActions(&translatedEvent, *e);
                QCoreApplication::sendEvent(**grabItem, &translatedEvent);
                e->setAccepted(translatedEvent.isAccepted());
                e->setDropAction(translatedEvent.dropAction());
                grabber->setTarget(**grabItem);
            }
        }
        if (event->type() != QEvent::DragMove) {    // Either an accepted drop or a leave.
            QDragLeaveEvent leaveEvent;
            for (; grabItem != grabber->end(); grabItem = grabber->release(grabItem))
                QCoreApplication::sendEvent(**grabItem, &leaveEvent);
            return;
        } else for (; grabItem != grabber->end(); grabItem = grabber->release(grabItem)) {
            QDragMoveEvent *moveEvent = static_cast<QDragMoveEvent *>(event);
            if (deliverDragEvent(grabber, **grabItem, moveEvent)) {
                moveEvent->setAccepted(true);
                for (++grabItem; grabItem != grabber->end();) {
                    QPointF p = (**grabItem)->mapFromScene(moveEvent->pos());
                    if ((**grabItem)->contains(p)) {
                        QDragMoveEvent translatedEvent(
                                p.toPoint(),
                                moveEvent->possibleActions(),
                                moveEvent->mimeData(),
                                moveEvent->mouseButtons(),
                                moveEvent->keyboardModifiers());
                        QQuickDropEventEx::copyActions(&translatedEvent, *moveEvent);
                        QCoreApplication::sendEvent(**grabItem, &translatedEvent);
                        ++grabItem;
                    } else {
                        QDragLeaveEvent leaveEvent;
                        QCoreApplication::sendEvent(**grabItem, &leaveEvent);
                        grabItem = grabber->release(grabItem);
                    }
                }
                return;
            } else {
                QDragLeaveEvent leaveEvent;
                QCoreApplication::sendEvent(**grabItem, &leaveEvent);
            }
        }
    }
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
        QDragMoveEvent *e = static_cast<QDragMoveEvent *>(event);
        QDragEnterEvent enterEvent(
                e->pos(),
                e->possibleActions(),
                e->mimeData(),
                e->mouseButtons(),
                e->keyboardModifiers());
        QQuickDropEventEx::copyActions(&enterEvent, *e);
        event->setAccepted(deliverDragEvent(grabber, contentItem, &enterEvent));
    }
}

bool QQuickWindowPrivate::deliverDragEvent(QQuickDragGrabber *grabber, QQuickItem *item, QDragMoveEvent *event)
{
    bool accepted = false;
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
    if (!item->isVisible() || !item->isEnabled() || QQuickItemPrivate::get(item)->culled)
        return false;
    QPointF p = item->mapFromScene(event->pos());
    bool itemContained = item->contains(p);

    if (!itemContained && itemPrivate->flags & QQuickItem::ItemClipsChildrenToShape) {
        return false;
    }

    QDragEnterEvent enterEvent(
            event->pos(),
            event->possibleActions(),
            event->mimeData(),
            event->mouseButtons(),
            event->keyboardModifiers());
    QQuickDropEventEx::copyActions(&enterEvent, *event);
    QList<QQuickItem *> children = itemPrivate->paintOrderChildItems();
    for (int ii = children.count() - 1; ii >= 0; --ii) {
        if (deliverDragEvent(grabber, children.at(ii), &enterEvent))
            return true;
    }

    if (itemContained) {
        if (event->type() == QEvent::DragMove || itemPrivate->flags & QQuickItem::ItemAcceptsDrops) {
            QDragMoveEvent translatedEvent(
                    p.toPoint(),
                    event->possibleActions(),
                    event->mimeData(),
                    event->mouseButtons(),
                    event->keyboardModifiers(),
                    event->type());
            QQuickDropEventEx::copyActions(&translatedEvent, *event);
            QCoreApplication::sendEvent(item, &translatedEvent);
            if (event->type() == QEvent::DragEnter) {
                if (translatedEvent.isAccepted()) {
                    grabber->grab(item);
                    accepted = true;
                }
            } else {
                accepted = true;
            }
        }
    }

    return accepted;
}
#endif // QT_NO_DRAGANDDROP

#ifndef QT_NO_CURSOR
void QQuickWindowPrivate::updateCursor(const QPointF &scenePos)
{
    Q_Q(QQuickWindow);

    QQuickItem *oldCursorItem = cursorItem;
    cursorItem = findCursorItem(contentItem, scenePos);

    if (cursorItem != oldCursorItem) {
        QWindow *renderWindow = QQuickRenderControl::renderWindowFor(q);
        QWindow *window = renderWindow ? renderWindow : q;
        if (cursorItem)
            window->setCursor(cursorItem->cursor());
        else
            window->unsetCursor();
    }
}

QQuickItem *QQuickWindowPrivate::findCursorItem(QQuickItem *item, const QPointF &scenePos)
{
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
    if (itemPrivate->flags & QQuickItem::ItemClipsChildrenToShape) {
        QPointF p = item->mapFromScene(scenePos);
        if (!item->contains(p))
            return 0;
    }

    if (itemPrivate->subtreeCursorEnabled) {
        QList<QQuickItem *> children = itemPrivate->paintOrderChildItems();
        for (int ii = children.count() - 1; ii >= 0; --ii) {
            QQuickItem *child = children.at(ii);
            if (!child->isVisible() || !child->isEnabled() || QQuickItemPrivate::get(child)->culled)
                continue;
            if (QQuickItem *cursorItem = findCursorItem(child, scenePos))
                return cursorItem;
        }
    }

    if (itemPrivate->hasCursor) {
        QPointF p = item->mapFromScene(scenePos);
        if (item->contains(p))
            return item;
    }
    return 0;
}
#endif

bool QQuickWindowPrivate::sendFilteredTouchEvent(QQuickItem *target, QQuickItem *item, QQuickPointerTouchEvent *event, QSet<QQuickItem *> *hasFiltered)
{
    Q_Q(QQuickWindow);

    if (!target)
        return false;

    bool filtered = false;

    QQuickItemPrivate *targetPrivate = QQuickItemPrivate::get(target);
    if (targetPrivate->filtersChildMouseEvents && !hasFiltered->contains(target)) {
        hasFiltered->insert(target);
        QScopedPointer<QTouchEvent> targetEvent(event->touchEventForItem(target, true));
        if (targetEvent) {
            if (target->childMouseEventFilter(item, targetEvent.data())) {
                qCDebug(DBG_TOUCH) << " - first chance intercepted on childMouseEventFilter by " << target;
                QVector<int> touchIds;
                const int touchPointCount = targetEvent->touchPoints().size();
                touchIds.reserve(touchPointCount);
                for (int i = 0; i < touchPointCount; ++i)
                    touchIds.append(targetEvent->touchPoints().at(i).id());
                target->grabTouchPoints(touchIds);
                if (q->mouseGrabberItem()) {
                    q->mouseGrabberItem()->ungrabMouse();
                    touchMouseId = -1;
                    touchMouseDevice = nullptr;
                }
                filtered = true;
            }

            for (int i = 0; i < targetEvent->touchPoints().size(); ++i) {
                const QTouchEvent::TouchPoint &tp = targetEvent->touchPoints().at(i);

                QEvent::Type t;
                switch (tp.state()) {
                case Qt::TouchPointPressed:
                    t = QEvent::MouseButtonPress;
                    break;
                case Qt::TouchPointReleased:
                    t = QEvent::MouseButtonRelease;
                    break;
                case Qt::TouchPointStationary:
                    continue;
                default:
                    t = QEvent::MouseMove;
                    break;
                }

                // Only deliver mouse event if it is the touchMouseId or it could become the touchMouseId
                if (touchMouseId == -1 || touchMouseId == tp.id()) {
                    // targetEvent is already transformed wrt local position, velocity, etc.

                    // FIXME: remove asTouchEvent!!!
                    QScopedPointer<QMouseEvent> mouseEvent(touchToMouseEvent(t, tp, event->asTouchEvent(), item, false));
                    if (target->childMouseEventFilter(item, mouseEvent.data())) {
                        qCDebug(DBG_TOUCH) << " - second chance intercepted on childMouseEventFilter by " << target;
                        if (t != QEvent::MouseButtonRelease) {
                            qCDebug(DBG_TOUCH_TARGET) << "TP" << tp.id() << "->" << target;
                            touchMouseId = tp.id();
                            touchMouseDevice = event->device();
                            touchMouseDevice->pointerEvent()->pointById(tp.id())->setGrabber(target);
                            target->grabMouse();
                        }
                        filtered = true;
                    }
                    // Only one event can be filtered as a mouse event.
                    break;
                }
            }
        }
    }

    return sendFilteredTouchEvent(target->parentItem(), item, event, hasFiltered) || filtered;
}

bool QQuickWindowPrivate::sendFilteredMouseEvent(QQuickItem *target, QQuickItem *item, QEvent *event, QSet<QQuickItem *> *hasFiltered)
{
    if (!target)
        return false;

    QQuickItemPrivate *targetPrivate = QQuickItemPrivate::get(target);
    if (targetPrivate->replayingPressEvent)
        return false;

    bool filtered = false;
    if (targetPrivate->filtersChildMouseEvents && !hasFiltered->contains(target)) {
        hasFiltered->insert(target);
        if (target->childMouseEventFilter(item, event))
            filtered = true;
        qCDebug(DBG_MOUSE_TARGET) << target << "childMouseEventFilter ->" << filtered;
    }

    return sendFilteredMouseEvent(target->parentItem(), item, event, hasFiltered) || filtered;
}

bool QQuickWindowPrivate::dragOverThreshold(qreal d, Qt::Axis axis, QMouseEvent *event, int startDragThreshold)
{
    QStyleHints *styleHints = QGuiApplication::styleHints();
    int caps = QGuiApplicationPrivate::mouseEventCaps(event);
    bool dragVelocityLimitAvailable = (caps & QTouchDevice::Velocity)
        && styleHints->startDragVelocity();
    bool overThreshold = qAbs(d) > (startDragThreshold >= 0 ? startDragThreshold : styleHints->startDragDistance());
    if (dragVelocityLimitAvailable) {
        QVector2D velocityVec = QGuiApplicationPrivate::mouseEventVelocity(event);
        qreal velocity = axis == Qt::XAxis ? velocityVec.x() : velocityVec.y();
        overThreshold |= qAbs(velocity) > styleHints->startDragVelocity();
    }
    return overThreshold;
}

bool QQuickWindowPrivate::dragOverThreshold(qreal d, Qt::Axis axis, const QTouchEvent::TouchPoint *tp, int startDragThreshold)
{
    QStyleHints *styleHints = qApp->styleHints();
    bool overThreshold = qAbs(d) > (startDragThreshold >= 0 ? startDragThreshold : styleHints->startDragDistance());
    qreal velocity = axis == Qt::XAxis ? tp->velocity().x() : tp->velocity().y();
    overThreshold |= qAbs(velocity) > styleHints->startDragVelocity();
    return overThreshold;
}

/*!
    \qmlproperty list<Object> Window::data
    \default

    The data property allows you to freely mix visual children, resources
    and other Windows in a Window.

    If you assign another Window to the data list, the nested window will
    become "transient for" the outer Window.

    If you assign an \l Item to the data list, it becomes a child of the
    Window's \l contentItem, so that it appears inside the window. The item's
    parent will be the window's contentItem, which is the root of the Item
    ownership tree within that Window.

    If you assign any other object type, it is added as a resource.

    It should not generally be necessary to refer to the \c data property,
    as it is the default property for Window and thus all child items are
    automatically assigned to this property.

    \sa QWindow::transientParent()
 */

void QQuickWindowPrivate::data_append(QQmlListProperty<QObject> *property, QObject *o)
{
    if (!o)
        return;
    QQuickWindow *that = static_cast<QQuickWindow *>(property->object);
    if (QQuickWindow *window = qmlobject_cast<QQuickWindow *>(o))
        window->setTransientParent(that);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(that->contentItem())->data();
    itemProperty.append(&itemProperty, o);
}

int QQuickWindowPrivate::data_count(QQmlListProperty<QObject> *property)
{
    QQuickWindow *win = static_cast<QQuickWindow*>(property->object);
    if (!win || !win->contentItem() || !QQuickItemPrivate::get(win->contentItem())->data().count)
        return 0;
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(win->contentItem())->data();
    return itemProperty.count(&itemProperty);
}

QObject *QQuickWindowPrivate::data_at(QQmlListProperty<QObject> *property, int i)
{
    QQuickWindow *win = static_cast<QQuickWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(win->contentItem())->data();
    return itemProperty.at(&itemProperty, i);
}

void QQuickWindowPrivate::data_clear(QQmlListProperty<QObject> *property)
{
    QQuickWindow *win = static_cast<QQuickWindow*>(property->object);
    QQmlListProperty<QObject> itemProperty = QQuickItemPrivate::get(win->contentItem())->data();
    itemProperty.clear(&itemProperty);
}

bool QQuickWindowPrivate::isRenderable() const
{
    Q_Q(const QQuickWindow);
    return ((q->isExposed() && q->isVisible())) && q->geometry().isValid();
}

void QQuickWindowPrivate::contextCreationFailureMessage(const QSurfaceFormat &format,
                                                        QString *translatedMessage,
                                                        QString *untranslatedMessage,
                                                        bool isEs)
{
    const QString contextType = QLatin1String(isEs ? "EGL" : "OpenGL");
    QString formatStr;
    QDebug(&formatStr) << format;
#if defined(Q_OS_WIN32)
    const bool isDebug = QLibraryInfo::isDebugBuild();
    const QString eglLibName = QLatin1String(isDebug ? "libEGLd.dll" : "libEGL.dll");
    const QString glesLibName = QLatin1String(isDebug ? "libGLESv2d.dll" : "libGLESv2.dll");
     //: %1 Context type (Open GL, EGL), %2 format, ANGLE %3, %4 library names
    const char msg[] = QT_TRANSLATE_NOOP("QQuickWindow",
        "Failed to create %1 context for format %2.\n"
        "This is most likely caused by not having the necessary graphics drivers installed.\n\n"
        "Install a driver providing OpenGL 2.0 or higher, or, if this is not possible, "
        "make sure the ANGLE Open GL ES 2.0 emulation libraries (%3, %4 and d3dcompiler_*.dll) "
        "are available in the application executable's directory or in a location listed in PATH.");
    *translatedMessage = QQuickWindow::tr(msg).arg(contextType, formatStr, eglLibName, glesLibName);
    *untranslatedMessage = QString::fromLatin1(msg).arg(contextType, formatStr, eglLibName, glesLibName);
#else // Q_OS_WIN32
    //: %1 Context type (Open GL, EGL), %2 format specification
    const char msg[] = QT_TRANSLATE_NOOP("QQuickWindow",
                                         "Failed to create %1 context for format %2");
    *translatedMessage = QQuickWindow::tr(msg).arg(contextType, formatStr);
    *untranslatedMessage = QString::fromLatin1(msg).arg(contextType, formatStr);
#endif // !Q_OS_WIN32
}

/*!
    Propagates an event \a e to a QQuickItem \a item on the window.

    Use \l QCoreApplication::sendEvent() directly instead.

    The return value is currently not used.

    \deprecated
*/
// ### Qt6: remove
bool QQuickWindow::sendEvent(QQuickItem *item, QEvent *e)
{
    Q_D(QQuickWindow);

    if (!item) {
        qWarning("QQuickWindow::sendEvent: Cannot send event to a null item");
        return false;
    }

    Q_ASSERT(e);

    switch (e->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        e->accept();
        QCoreApplication::sendEvent(item, e);
        while (!e->isAccepted() && (item = item->parentItem())) {
            e->accept();
            QCoreApplication::sendEvent(item, e);
        }
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove: {
            // XXX todo - should sendEvent be doing this?  how does it relate to forwarded events?
            QSet<QQuickItem *> hasFiltered;
            if (!d->sendFilteredMouseEvent(item->parentItem(), item, e, &hasFiltered)) {
                // accept because qml items by default accept and have to explicitly opt out of accepting
                e->accept();
                QCoreApplication::sendEvent(item, e);
            }
        }
        break;
    default:
        QCoreApplication::sendEvent(item, e);
        break;
    }

    return false;
}

void QQuickWindowPrivate::cleanupNodes()
{
    for (int ii = 0; ii < cleanupNodeList.count(); ++ii)
        delete cleanupNodeList.at(ii);
    cleanupNodeList.clear();
}

void QQuickWindowPrivate::cleanupNodesOnShutdown(QQuickItem *item)
{
    QQuickItemPrivate *p = QQuickItemPrivate::get(item);
    if (p->itemNodeInstance) {
        delete p->itemNodeInstance;
        p->itemNodeInstance = 0;

        if (p->extra.isAllocated()) {
            p->extra->opacityNode = 0;
            p->extra->clipNode = 0;
            p->extra->rootNode = 0;
        }

        p->paintNode = 0;

        p->dirty(QQuickItemPrivate::Window);
    }

    // Qt 6: Make invalidateSceneGraph a virtual member of QQuickItem
    if (p->flags & QQuickItem::ItemHasContents) {
        const QMetaObject *mo = item->metaObject();
        int index = mo->indexOfSlot("invalidateSceneGraph()");
        if (index >= 0) {
            const QMetaMethod &method = mo->method(index);
            // Skip functions named invalidateSceneGraph() in QML items.
            if (strstr(method.enclosingMetaObject()->className(), "_QML_") == 0)
                method.invoke(item, Qt::DirectConnection);
        }
    }

    for (int ii = 0; ii < p->childItems.count(); ++ii)
        cleanupNodesOnShutdown(p->childItems.at(ii));
}

// This must be called from the render thread, with the main thread frozen
void QQuickWindowPrivate::cleanupNodesOnShutdown()
{
    Q_Q(QQuickWindow);
    cleanupNodes();
    cleanupNodesOnShutdown(contentItem);
    for (QSet<QQuickItem *>::const_iterator it = parentlessItems.begin(), cend = parentlessItems.end(); it != cend; ++it)
        cleanupNodesOnShutdown(*it);
    animationController->windowNodesDestroyed();
    q->cleanupSceneGraph();
}

void QQuickWindowPrivate::updateDirtyNodes()
{
    qCDebug(DBG_DIRTY) << "QQuickWindowPrivate::updateDirtyNodes():";

    cleanupNodes();

    QQuickItem *updateList = dirtyItemList;
    dirtyItemList = 0;
    if (updateList) QQuickItemPrivate::get(updateList)->prevDirtyItem = &updateList;

    while (updateList) {
        QQuickItem *item = updateList;
        QQuickItemPrivate *itemPriv = QQuickItemPrivate::get(item);
        itemPriv->removeFromDirtyList();

        qCDebug(DBG_DIRTY) << "   QSGNode:" << item << qPrintable(itemPriv->dirtyToString());
        updateDirtyNode(item);
    }
}

static inline QSGNode *qquickitem_before_paintNode(QQuickItemPrivate *d)
{
    const QList<QQuickItem *> childItems = d->paintOrderChildItems();
    QQuickItem *before = 0;
    for (int i=0; i<childItems.size(); ++i) {
        QQuickItemPrivate *dd = QQuickItemPrivate::get(childItems.at(i));
        // Perform the same check as the in fetchNextNode below.
        if (dd->z() < 0 && (dd->explicitVisible || (dd->extra.isAllocated() && dd->extra->effectRefCount)))
            before = childItems.at(i);
        else
            break;
    }
    return Q_UNLIKELY(before) ? QQuickItemPrivate::get(before)->itemNode() : 0;
}

static QSGNode *fetchNextNode(QQuickItemPrivate *itemPriv, int &ii, bool &returnedPaintNode)
{
    QList<QQuickItem *> orderedChildren = itemPriv->paintOrderChildItems();

    for (; ii < orderedChildren.count() && orderedChildren.at(ii)->z() < 0; ++ii) {
        QQuickItemPrivate *childPrivate = QQuickItemPrivate::get(orderedChildren.at(ii));
        if (!childPrivate->explicitVisible &&
            (!childPrivate->extra.isAllocated() || !childPrivate->extra->effectRefCount))
            continue;

        ii++;
        return childPrivate->itemNode();
    }

    if (itemPriv->paintNode && !returnedPaintNode) {
        returnedPaintNode = true;
        return itemPriv->paintNode;
    }

    for (; ii < orderedChildren.count(); ++ii) {
        QQuickItemPrivate *childPrivate = QQuickItemPrivate::get(orderedChildren.at(ii));
        if (!childPrivate->explicitVisible &&
            (!childPrivate->extra.isAllocated() || !childPrivate->extra->effectRefCount))
            continue;

        ii++;
        return childPrivate->itemNode();
    }

    return 0;
}

void QQuickWindowPrivate::updateDirtyNode(QQuickItem *item)
{
    QQuickItemPrivate *itemPriv = QQuickItemPrivate::get(item);
    quint32 dirty = itemPriv->dirtyAttributes;
    itemPriv->dirtyAttributes = 0;

    if ((dirty & QQuickItemPrivate::TransformUpdateMask) ||
        (dirty & QQuickItemPrivate::Size && itemPriv->origin() != QQuickItem::TopLeft &&
         (itemPriv->scale() != 1. || itemPriv->rotation() != 0.))) {

        QMatrix4x4 matrix;

        if (itemPriv->x != 0. || itemPriv->y != 0.)
            matrix.translate(itemPriv->x, itemPriv->y);

        for (int ii = itemPriv->transforms.count() - 1; ii >= 0; --ii)
            itemPriv->transforms.at(ii)->applyTo(&matrix);

        if (itemPriv->scale() != 1. || itemPriv->rotation() != 0.) {
            QPointF origin = item->transformOriginPoint();
            matrix.translate(origin.x(), origin.y());
            if (itemPriv->scale() != 1.)
                matrix.scale(itemPriv->scale(), itemPriv->scale());
            if (itemPriv->rotation() != 0.)
                matrix.rotate(itemPriv->rotation(), 0, 0, 1);
            matrix.translate(-origin.x(), -origin.y());
        }

        itemPriv->itemNode()->setMatrix(matrix);
    }

    bool clipEffectivelyChanged = (dirty & (QQuickItemPrivate::Clip | QQuickItemPrivate::Window)) &&
                                  ((item->clip() == false) != (itemPriv->clipNode() == 0));
    int effectRefCount = itemPriv->extra.isAllocated()?itemPriv->extra->effectRefCount:0;
    bool effectRefEffectivelyChanged = (dirty & (QQuickItemPrivate::EffectReference | QQuickItemPrivate::Window)) &&
                                  ((effectRefCount == 0) != (itemPriv->rootNode() == 0));

    if (clipEffectivelyChanged) {
        QSGNode *parent = itemPriv->opacityNode() ? (QSGNode *) itemPriv->opacityNode() :
                                                    (QSGNode *) itemPriv->itemNode();
        QSGNode *child = itemPriv->rootNode();

        if (item->clip()) {
            Q_ASSERT(itemPriv->clipNode() == 0);
            QQuickDefaultClipNode *clip = new QQuickDefaultClipNode(item->clipRect());
            itemPriv->extra.value().clipNode = clip;
            clip->update();

            if (!child) {
                parent->reparentChildNodesTo(clip);
                parent->appendChildNode(clip);
            } else {
                parent->removeChildNode(child);
                clip->appendChildNode(child);
                parent->appendChildNode(clip);
            }

        } else {
            QQuickDefaultClipNode *clip = itemPriv->clipNode();
            Q_ASSERT(clip);
            parent->removeChildNode(clip);
            if (child) {
                clip->removeChildNode(child);
                parent->appendChildNode(child);
            } else {
                clip->reparentChildNodesTo(parent);
            }

            delete itemPriv->clipNode();
            itemPriv->extra->clipNode = 0;
        }
    }

    if (effectRefEffectivelyChanged) {
        if (dirty & QQuickItemPrivate::ChildrenUpdateMask)
            itemPriv->childContainerNode()->removeAllChildNodes();

        QSGNode *parent = itemPriv->clipNode();
        if (!parent)
            parent = itemPriv->opacityNode();
        if (!parent)
            parent = itemPriv->itemNode();

        if (itemPriv->extra.isAllocated() && itemPriv->extra->effectRefCount) {
            Q_ASSERT(itemPriv->rootNode() == 0);
            QSGRootNode *root = new QSGRootNode();
            itemPriv->extra->rootNode = root;
            parent->reparentChildNodesTo(root);
            parent->appendChildNode(root);
        } else {
            Q_ASSERT(itemPriv->rootNode() != 0);
            QSGRootNode *root = itemPriv->rootNode();
            parent->removeChildNode(root);
            root->reparentChildNodesTo(parent);
            delete itemPriv->rootNode();
            itemPriv->extra->rootNode = 0;
        }
    }

    if (dirty & QQuickItemPrivate::ChildrenUpdateMask) {
        int ii = 0;
        bool fetchedPaintNode = false;
        QList<QQuickItem *> orderedChildren = itemPriv->paintOrderChildItems();
        int desiredNodesSize = orderedChildren.size() + (itemPriv->paintNode ? 1 : 0);

        // now start making current state match the promised land of
        // desiredNodes. in the case of our current state matching desiredNodes
        // (though why would we get ChildrenUpdateMask with no changes?) then we
        // should make no changes at all.

        // how many nodes did we process, when examining changes
        int desiredNodesProcessed = 0;

        // currentNode is how far, in our present tree, we have processed. we
        // make use of this later on to trim the current child list if the
        // desired list is shorter.
        QSGNode *groupNode = itemPriv->childContainerNode();
        QSGNode *currentNode = groupNode->firstChild();
        int added = 0;
        int removed = 0;
        int replaced = 0;
        QSGNode *desiredNode = 0;

        while (currentNode && (desiredNode = fetchNextNode(itemPriv, ii, fetchedPaintNode))) {
            // uh oh... reality and our utopic paradise are diverging!
            // we need to reconcile this...
            if (currentNode != desiredNode) {
                // for now, we're just removing the node from the children -
                // and replacing it with the new node.
                if (desiredNode->parent())
                    desiredNode->parent()->removeChildNode(desiredNode);
                groupNode->insertChildNodeAfter(desiredNode, currentNode);
                groupNode->removeChildNode(currentNode);
                replaced++;

                // since we just replaced currentNode, we also need to reset
                // the pointer.
                currentNode = desiredNode;
            }

            currentNode = currentNode->nextSibling();
            desiredNodesProcessed++;
        }

        // if we didn't process as many nodes as in the new list, then we have
        // more nodes at the end of desiredNodes to append to our list.
        // this will be the case when adding new nodes, for instance.
        if (desiredNodesProcessed < desiredNodesSize) {
            while ((desiredNode = fetchNextNode(itemPriv, ii, fetchedPaintNode))) {
                if (desiredNode->parent())
                    desiredNode->parent()->removeChildNode(desiredNode);
                groupNode->appendChildNode(desiredNode);
                added++;
            }
        } else if (currentNode) {
            // on the other hand, if we processed less than our current node
            // tree, then nodes have been _removed_ from the scene, and we need
            // to take care of that here.
            while (currentNode) {
                QSGNode *node = currentNode->nextSibling();
                groupNode->removeChildNode(currentNode);
                currentNode = node;
                removed++;
            }
        }
    }

    if ((dirty & QQuickItemPrivate::Size) && itemPriv->clipNode()) {
        itemPriv->clipNode()->setRect(item->clipRect());
        itemPriv->clipNode()->update();
    }

    if (dirty & (QQuickItemPrivate::OpacityValue | QQuickItemPrivate::Visible
                 | QQuickItemPrivate::HideReference | QQuickItemPrivate::Window))
    {
        qreal opacity = itemPriv->explicitVisible && (!itemPriv->extra.isAllocated() || itemPriv->extra->hideRefCount == 0)
                      ? itemPriv->opacity() : qreal(0);

        if (opacity != 1 && !itemPriv->opacityNode()) {
            QSGOpacityNode *node = new QSGOpacityNode;
            itemPriv->extra.value().opacityNode = node;

            QSGNode *parent = itemPriv->itemNode();
            QSGNode *child = itemPriv->clipNode();
            if (!child)
                child = itemPriv->rootNode();

            if (child) {
                parent->removeChildNode(child);
                node->appendChildNode(child);
                parent->appendChildNode(node);
            } else {
                parent->reparentChildNodesTo(node);
                parent->appendChildNode(node);
            }
        }
        if (itemPriv->opacityNode())
            itemPriv->opacityNode()->setOpacity(opacity);
    }

    if (dirty & QQuickItemPrivate::ContentUpdateMask) {

        if (itemPriv->flags & QQuickItem::ItemHasContents) {
            updatePaintNodeData.transformNode = itemPriv->itemNode();
            itemPriv->paintNode = item->updatePaintNode(itemPriv->paintNode, &updatePaintNodeData);

            Q_ASSERT(itemPriv->paintNode == 0 ||
                     itemPriv->paintNode->parent() == 0 ||
                     itemPriv->paintNode->parent() == itemPriv->childContainerNode());

            if (itemPriv->paintNode && itemPriv->paintNode->parent() == 0) {
                QSGNode *before = qquickitem_before_paintNode(itemPriv);
                if (before && before->parent()) {
                    Q_ASSERT(before->parent() == itemPriv->childContainerNode());
                    itemPriv->childContainerNode()->insertChildNodeAfter(itemPriv->paintNode, before);
                } else {
                    itemPriv->childContainerNode()->prependChildNode(itemPriv->paintNode);
                }
            }
        } else if (itemPriv->paintNode) {
            delete itemPriv->paintNode;
            itemPriv->paintNode = 0;
        }
    }

#ifndef QT_NO_DEBUG
    // Check consistency.

    QList<QSGNode *> nodes;
    nodes << itemPriv->itemNodeInstance
          << itemPriv->opacityNode()
          << itemPriv->clipNode()
          << itemPriv->rootNode()
          << itemPriv->paintNode;
    nodes.removeAll(0);

    Q_ASSERT(nodes.constFirst() == itemPriv->itemNodeInstance);
    for (int i=1; i<nodes.size(); ++i) {
        QSGNode *n = nodes.at(i);
        // Failing this means we messed up reparenting
        Q_ASSERT(n->parent() == nodes.at(i-1));
        // Only the paintNode and the one who is childContainer may have more than one child.
        Q_ASSERT(n == itemPriv->paintNode || n == itemPriv->childContainerNode() || n->childCount() == 1);
    }
#endif

}

bool QQuickWindowPrivate::emitError(QQuickWindow::SceneGraphError error, const QString &msg)
{
    Q_Q(QQuickWindow);
    static const QMetaMethod errorSignal = QMetaMethod::fromSignal(&QQuickWindow::sceneGraphError);
    if (q->isSignalConnected(errorSignal)) {
        emit q->sceneGraphError(error, msg);
        return true;
    }
    return false;
}

void QQuickWindow::maybeUpdate()
{
    Q_D(QQuickWindow);
    if (d->renderControl)
        QQuickRenderControlPrivate::get(d->renderControl)->maybeUpdate();
    else if (d->windowManager)
        d->windowManager->maybeUpdate(this);
}

void QQuickWindow::cleanupSceneGraph()
{
    Q_D(QQuickWindow);
#ifndef QT_NO_OPENGL
    delete d->vaoHelper;
    d->vaoHelper = 0;
#endif
    if (!d->renderer)
        return;

    delete d->renderer->rootNode();
    delete d->renderer;
    d->renderer = 0;

    d->runAndClearJobs(&d->beforeSynchronizingJobs);
    d->runAndClearJobs(&d->afterSynchronizingJobs);
    d->runAndClearJobs(&d->beforeRenderingJobs);
    d->runAndClearJobs(&d->afterRenderingJobs);
    d->runAndClearJobs(&d->afterSwapJobs);
}

void QQuickWindow::setTransientParent_helper(QQuickWindow *window)
{
    setTransientParent(window);
    disconnect(sender(), SIGNAL(windowChanged(QQuickWindow*)),
               this, SLOT(setTransientParent_helper(QQuickWindow*)));
}

/*!
    Returns the OpenGL context used for rendering.

    If the scene graph is not ready, or the scene graph is not using OpenGL,
    this function will return null.

    \note If using a scene graph adaptation other than OpenGL this
    function will return nullptr.

    \sa sceneGraphInitialized(), sceneGraphInvalidated()
 */

QOpenGLContext *QQuickWindow::openglContext() const
{
#ifndef QT_NO_OPENGL
    Q_D(const QQuickWindow);
    if (d->context && d->context->isValid()) {
        QSGRendererInterface *rif = d->context->sceneGraphContext()->rendererInterface(d->context);
        if (rif && rif->graphicsApi() == QSGRendererInterface::OpenGL) {
            auto openglRenderContext = static_cast<const QSGDefaultRenderContext *>(d->context);
            return openglRenderContext->openglContext();
        }
    }
#endif
    return nullptr;
}

/*!
    Returns true if the scene graph has been initialized; otherwise returns false.
 */
bool QQuickWindow::isSceneGraphInitialized() const
{
    Q_D(const QQuickWindow);
    return d->context != 0 && d->context->isValid();
}

/*!
    \fn void QQuickWindow::frameSwapped()

    This signal is emitted when a frame has been queued for presenting. With
    vertical synchronization enabled the signal is emitted at most once per
    vsync interval in a continuously animating scene.

    This signal will be emitted from the scene graph rendering thread.
*/


/*!
    \fn void QQuickWindow::sceneGraphInitialized()

    This signal is emitted when the scene graph has been initialized.

    This signal will be emitted from the scene graph rendering thread.

 */


/*!
    \fn void QQuickWindow::sceneGraphInvalidated()

    This signal is emitted when the scene graph has been invalidated.

    This signal implies that the graphics rendering context used
    has been invalidated and all user resources tied to that context
    should be released.

    In the case of the default OpenGL adaptation the context of this
    window will be bound when this function is called. The only exception
    is if the native OpenGL has been destroyed outside Qt's control,
    for instance through EGL_CONTEXT_LOST.

    This signal will be emitted from the scene graph rendering thread.
 */

/*!
    \fn void QQuickWindow::sceneGraphError(SceneGraphError error, const QString &message)

    This signal is emitted when an \a error occurred during scene graph initialization.

    Applications should connect to this signal if they wish to handle errors,
    like graphics context creation failures, in a custom way. When no slot is
    connected to the signal, the behavior will be different: Quick will print
    the \a message, or show a message box, and terminate the application.

    This signal will be emitted from the gui thread.

    \since 5.3
 */

/*!
    \class QQuickCloseEvent
    \internal
    \since 5.1

    \inmodule QtQuick

    \brief Notification that a \l QQuickWindow is about to be closed
*/
/*!
    \qmltype CloseEvent
    \instantiates QQuickCloseEvent
    \inqmlmodule QtQuick.Window
    \ingroup qtquick-visual
    \brief Notification that a \l Window is about to be closed
    \since 5.1

    Notification that a window is about to be closed by the windowing system
    (e.g. the user clicked the title bar close button). The CloseEvent contains
    an accepted property which can be set to false to abort closing the window.

    \sa QQuickWindow::closing()
*/

/*!
    \qmlproperty bool CloseEvent::accepted

    This property indicates whether the application will allow the user to
    close the window.  It is true by default.
*/

/*!
    \fn void QQuickWindow::closing(QQuickCloseEvent *close)
    \since 5.1

    This signal is emitted when the window receives the event \a close from
    the windowing system.
*/

/*!
    \qmlsignal QtQuick.Window::Window::closing(CloseEvent close)
    \since 5.1

    This signal is emitted when the user tries to close the window.

    This signal includes a \a close parameter. The \c {close.accepted}
    property is true by default so that the window is allowed to close; but you
    can implement an \c onClosing handler and set \c {close.accepted = false} if
    you need to do something else before the window can be closed.

    The corresponding handler is \c onClosing.
 */

#ifndef QT_NO_OPENGL
/*!
    Sets the render target for this window to be \a fbo.

    The specified fbo must be created in the context of the window
    or one that shares with it.

    \note
    This function only has an effect when using the default OpenGL scene
    graph adaptation.

    \warning
    This function can only be called from the thread doing
    the rendering.
 */

void QQuickWindow::setRenderTarget(QOpenGLFramebufferObject *fbo)
{
    Q_D(QQuickWindow);
    if (d->context && QThread::currentThread() != d->context->thread()) {
        qWarning("QQuickWindow::setRenderThread: Cannot set render target from outside the rendering thread");
        return;
    }

    d->renderTarget = fbo;
    if (fbo) {
        d->renderTargetId = fbo->handle();
        d->renderTargetSize = fbo->size();
    } else {
        d->renderTargetId = 0;
        d->renderTargetSize = QSize();
    }
}
#endif
/*!
    \overload

    Sets the render target for this window to be an FBO with
    \a fboId and \a size.

    The specified FBO must be created in the context of the window
    or one that shares with it.

    \note
    This function only has an effect when using the default OpenGL scene
    graph adaptation.

    \warning
    This function can only be called from the thread doing
    the rendering.
*/

void QQuickWindow::setRenderTarget(uint fboId, const QSize &size)
{
    Q_D(QQuickWindow);
    if (d->context && QThread::currentThread() != d->context->thread()) {
        qWarning("QQuickWindow::setRenderThread: Cannot set render target from outside the rendering thread");
        return;
    }

    d->renderTargetId = fboId;
    d->renderTargetSize = size;

    // Unset any previously set instance...
    d->renderTarget = 0;
}


/*!
    Returns the FBO id of the render target when set; otherwise returns 0.
 */
uint QQuickWindow::renderTargetId() const
{
    Q_D(const QQuickWindow);
    return d->renderTargetId;
}

/*!
    Returns the size of the currently set render target; otherwise returns an empty size.
 */
QSize QQuickWindow::renderTargetSize() const
{
    Q_D(const QQuickWindow);
    return d->renderTargetSize;
}



#ifndef QT_NO_OPENGL
/*!
    Returns the render target for this window.

    The default is to render to the surface of the window, in which
    case the render target is 0.

    \note
    This function will return nullptr when not using the OpenGL scene
    graph adaptation.
 */
QOpenGLFramebufferObject *QQuickWindow::renderTarget() const
{
    Q_D(const QQuickWindow);
    return d->renderTarget;
}
#endif

/*!
    Grabs the contents of the window and returns it as an image.

    It is possible to call the grabWindow() function when the window is not
    visible. This requires that the window is \l{QWindow::create()} {created}
    and has a valid size and that no other QQuickWindow instances are rendering
    in the same process.

    \warning Calling this function will cause performance problems.

    \warning This function can only be called from the GUI thread.
 */
QImage QQuickWindow::grabWindow()
{
    Q_D(QQuickWindow);

    if (!isVisible() && !d->renderControl) {
        if (d->windowManager && (d->windowManager->flags() & QSGRenderLoop::SupportsGrabWithoutExpose))
            return d->windowManager->grab(this);
    }

#ifndef QT_NO_OPENGL
    if (!isVisible() && !d->renderControl) {
        auto openglRenderContext = static_cast<QSGDefaultRenderContext *>(d->context);
        if (!openglRenderContext->openglContext()) {
            if (!handle() || !size().isValid()) {
                qWarning("QQuickWindow::grabWindow: window must be created and have a valid size");
                return QImage();
            }

            QOpenGLContext context;
            context.setFormat(requestedFormat());
            context.setShareContext(qt_gl_global_share_context());
            context.create();
            context.makeCurrent(this);
            d->context->initialize(&context);

            d->polishItems();
            d->syncSceneGraph();
            d->renderSceneGraph(size());

            bool alpha = format().alphaBufferSize() > 0 && color().alpha() < 255;
            QImage image = qt_gl_read_framebuffer(size() * effectiveDevicePixelRatio(), alpha, alpha);
            d->cleanupNodesOnShutdown();
            d->context->invalidate();
            context.doneCurrent();

            return image;
        }
    }
#endif
    if (d->renderControl)
        return d->renderControl->grab();
    else if (d->windowManager)
        return d->windowManager->grab(this);
    return QImage();
}

/*!
    Returns an incubation controller that splices incubation between frames
    for this window. QQuickView automatically installs this controller for you,
    otherwise you will need to install it yourself using \l{QQmlEngine::setIncubationController()}.

    The controller is owned by the window and will be destroyed when the window
    is deleted.
*/
QQmlIncubationController *QQuickWindow::incubationController() const
{
    Q_D(const QQuickWindow);

    if (!d->windowManager)
        return 0; // TODO: make sure that this is safe

    if (!d->incubationController)
        d->incubationController = new QQuickWindowIncubationController(d->windowManager);
    return d->incubationController;
}



/*!
    \enum QQuickWindow::CreateTextureOption

    The CreateTextureOption enums are used to customize a texture is wrapped.

    \value TextureHasAlphaChannel The texture has an alpha channel and should
    be drawn using blending.

    \value TextureHasMipmaps The texture has mipmaps and can be drawn with
    mipmapping enabled.

    \value TextureOwnsGLTexture The texture object owns the texture id and
    will delete the OpenGL texture when the texture object is deleted.

    \value TextureCanUseAtlas The image can be uploaded into a texture atlas.

    \value TextureIsOpaque The texture will return false for
    QSGTexture::hasAlphaChannel() and will not be blended. This flag was added
    in Qt 5.6.

 */

/*!
    \enum QQuickWindow::SceneGraphError

    This enum describes the error in a sceneGraphError() signal.

    \value ContextNotAvailable graphics context creation failed. This typically means that
    no suitable OpenGL implementation was found, for example because no graphics drivers
    are installed and so no OpenGL 2 support is present. On mobile and embedded boards
    that use OpenGL ES such an error is likely to indicate issues in the windowing system
    integration and possibly an incorrect configuration of Qt.

    \since 5.3
 */

/*!
    \fn void QQuickWindow::beforeSynchronizing()

    This signal is emitted before the scene graph is synchronized with the QML state.

    This signal can be used to do any preparation required before calls to
    QQuickItem::updatePaintNode().

    The OpenGL context used for rendering the scene graph will be bound at this point.

    \warning This signal is emitted from the scene graph rendering thread. If your
    slot function needs to finish before execution continues, you must make sure that
    the connection is direct (see Qt::ConnectionType).

    \warning Make very sure that a signal handler for beforeSynchronizing leaves the GL
    context in the same state as it was when the signal handler was entered. Failing to
    do so can result in the scene not rendering properly.

    \sa resetOpenGLState()
*/

/*!
    \fn void QQuickWindow::afterSynchronizing()

    This signal is emitted after the scene graph is synchronized with the QML state.

    This signal can be used to do preparation required after calls to
    QQuickItem::updatePaintNode(), while the GUI thread is still locked.

    The graphics context used for rendering the scene graph will be bound at this point.

    \warning This signal is emitted from the scene graph rendering thread. If your
    slot function needs to finish before execution continues, you must make sure that
    the connection is direct (see Qt::ConnectionType).

    \warning When using the OpenGL adaptation, make sure that a signal handler for
    afterSynchronizing leaves the OpenGL context in the same state as it was when the
    signal handler was entered. Failing to do so can result in the scene not rendering
    properly.

    \since 5.3
    \sa resetOpenGLState()
 */

/*!
    \fn void QQuickWindow::beforeRendering()

    This signal is emitted before the scene starts rendering.

    Combined with the modes for clearing the background, this option
    can be used to paint using raw OpenGL under QML content.

    The OpenGL context used for rendering the scene graph will be bound
    at this point.

    \warning This signal is emitted from the scene graph rendering thread. If your
    slot function needs to finish before execution continues, you must make sure that
    the connection is direct (see Qt::ConnectionType).

    \warning Make very sure that a signal handler for beforeRendering leaves the OpenGL
    context in the same state as it was when the signal handler was entered. Failing to
    do so can result in the scene not rendering properly.

    \sa resetOpenGLState()
*/

/*!
    \fn void QQuickWindow::afterRendering()

    This signal is emitted after the scene has completed rendering, before swapbuffers is called.

    This signal can be used to paint using raw OpenGL on top of QML content,
    or to do screen scraping of the current frame buffer.

    The OpenGL context used for rendering the scene graph will be bound at this point.

    \warning This signal is emitted from the scene graph rendering thread. If your
    slot function needs to finish before execution continues, you must make sure that
    the connection is direct (see Qt::ConnectionType).

    \warning Make very sure that a signal handler for afterRendering() leaves the OpenGL
    context in the same state as it was when the signal handler was entered. Failing to
    do so can result in the scene not rendering properly.

    \sa resetOpenGLState()
 */

/*!
    \fn void QQuickWindow::afterAnimating()

    This signal is emitted on the gui thread before requesting the render thread to
    perform the synchronization of the scene graph.

    Unlike the other similar signals, this one is emitted on the gui thread instead
    of the render thread. It can be used to synchronize external animation systems
    with the QML content.

    \since 5.3
 */

/*!
    \fn void QQuickWindow::openglContextCreated(QOpenGLContext *context)

    This signal is emitted on the gui thread when the OpenGL \a context
    for this window is created, before it is made current.

    Some implementations will share the same OpenGL context between
    multiple QQuickWindow instances. The openglContextCreated() signal
    will in this case only be emitted for the first window, when the
    OpenGL context is actually created.

    QQuickWindow::openglContext() will still return 0 for this window
    until after the QQuickWindow::sceneGraphInitialize() has been
    emitted.

    \note
    This signal will only be emmited when using the default OpenGL scene
    graph adaptation.

    \since 5.3
 */

/*!
    \fn void QQuickWindow::sceneGraphAboutToStop()

    This signal is emitted on the render thread when the scene graph is
    about to stop rendering. This happens usually because the window
    has been hidden.

    Applications may use this signal to release resources, but should be
    prepared to reinstantiated them again fast. The scene graph and the
    graphics context are not released at this time.

    \warning This signal is emitted from the scene graph rendering thread. If your
    slot function needs to finish before execution continues, you must make sure that
    the connection is direct (see Qt::ConnectionType).

    \warning Make very sure that a signal handler for sceneGraphAboutToStop() leaves the
    graphics context in the same state as it was when the signal handler was entered.
    Failing to do so can result in the scene not rendering properly.

    \sa sceneGraphInvalidated(), resetOpenGLState()
    \since 5.3
 */


/*!
    Sets whether the scene graph rendering of QML should clear the color buffer
    before it starts rendering to \a enabled.

    By disabling clearing of the color buffer, it is possible to render OpengGL content
    under the scene graph.

    The color buffer is cleared by default.

    \sa beforeRendering()
 */

void QQuickWindow::setClearBeforeRendering(bool enabled)
{
    Q_D(QQuickWindow);
    d->clearBeforeRendering = enabled;
}



/*!
    Returns whether clearing of the color buffer is done before rendering or not.
 */

bool QQuickWindow::clearBeforeRendering() const
{
    Q_D(const QQuickWindow);
    return d->clearBeforeRendering;
}

/*!
    \overload
 */

QSGTexture *QQuickWindow::createTextureFromImage(const QImage &image) const
{
    return createTextureFromImage(image, 0);
}


/*!
    Creates a new QSGTexture from the supplied \a image. If the image has an
    alpha channel, the corresponding texture will have an alpha channel.

    The caller of the function is responsible for deleting the returned texture.
    For example whe using the OpenGL adaptation the actual OpenGL texture will
    be deleted when the texture object is deleted.

    When \a options contains TextureCanUseAtlas, the engine may put the image
    into a texture atlas. Textures in an atlas need to rely on
    QSGTexture::normalizedTextureSubRect() for their geometry and will not
    support QSGTexture::Repeat. Other values from CreateTextureOption are
    ignored.

    When \a options contains TextureIsOpaque, the engine will create an RGB
    texture which returns false for QSGTexture::hasAlphaChannel(). Opaque
    textures will in most cases be faster to render. When this flag is not set,
    the texture will have an alpha channel based on the image's format.

    When \a options contains TextureHasMipmaps, the engine will create a
    texture which can use mipmap filtering. Mipmapped textures can not be in
    an atlas.

    When using the OpenGL adaptation, the returned texture will be using
    \c GL_TEXTURE_2D as texture target and \c GL_RGBA as internal format.
    Reimplement QSGTexture to create textures with different parameters.

    \warning This function will return 0 if the scene graph has not yet been
    initialized.

    \warning The returned texture is not memory managed by the scene graph and
    must be explicitly deleted by the caller on the rendering thread.
    This is achieved by deleting the texture from a QSGNode destructor
    or by using deleteLater() in the case where the texture already has affinity
    to the rendering thread.

    This function can be called from any thread.

    \sa sceneGraphInitialized(), QSGTexture
 */

QSGTexture *QQuickWindow::createTextureFromImage(const QImage &image, CreateTextureOptions options) const
{
    Q_D(const QQuickWindow);
    if (!isSceneGraphInitialized()) // check both for d->context and d->context->isValid()
         return 0;
    uint flags = 0;
    if (options & TextureCanUseAtlas)     flags |= QSGRenderContext::CreateTexture_Atlas;
    if (options & TextureHasMipmaps)      flags |= QSGRenderContext::CreateTexture_Mipmap;
    if (!(options & TextureIsOpaque))     flags |= QSGRenderContext::CreateTexture_Alpha;
    return d->context->createTexture(image, flags);
}



/*!
    Creates a new QSGTexture object from an existing OpenGL texture \a id and \a size.

    The caller of the function is responsible for deleting the returned texture.

    The returned texture will be using \c GL_TEXTURE_2D as texture target and
    assumes that internal format is \c {GL_RGBA}. Reimplement QSGTexture to
    create textures with different parameters.

    Use \a options to customize the texture attributes. The TextureUsesAtlas
    option is ignored.

    \warning This function will return null if the scenegraph has not yet been
    initialized or OpenGL is not in use.

    \note This function only has an effect when using the default OpenGL scene graph
    adpation.

    \sa sceneGraphInitialized(), QSGTexture
 */
QSGTexture *QQuickWindow::createTextureFromId(uint id, const QSize &size, CreateTextureOptions options) const
{
#ifndef QT_NO_OPENGL
    if (openglContext()) {
        QSGPlainTexture *texture = new QSGPlainTexture();
        texture->setTextureId(id);
        texture->setHasAlphaChannel(options & TextureHasAlphaChannel);
        texture->setOwnsTexture(options & TextureOwnsGLTexture);
        texture->setTextureSize(size);
        return texture;
    }
#else
    Q_UNUSED(id)
    Q_UNUSED(size)
    Q_UNUSED(options)
#endif
    return 0;
}

/*!
    \qmlproperty color Window::color

    The background color for the window.

    Setting this property is more efficient than using a separate Rectangle.
*/

/*!
    \property QQuickWindow::color
    \brief The color used to clear the OpenGL context.

    Setting the clear color has no effect when clearing is disabled.
    By default, the clear color is white.

    \sa setClearBeforeRendering(), setDefaultAlphaBuffer()
 */

void QQuickWindow::setColor(const QColor &color)
{
    Q_D(QQuickWindow);
    if (color == d->clearColor)
        return;

    if (color.alpha() != d->clearColor.alpha()) {
        QSurfaceFormat fmt = requestedFormat();
        if (color.alpha() < 255)
            fmt.setAlphaBufferSize(8);
        else
            fmt.setAlphaBufferSize(-1);
        setFormat(fmt);
    }
    d->clearColor = color;
    emit colorChanged(color);
    update();
}

QColor QQuickWindow::color() const
{
    return d_func()->clearColor;
}

/*!
    \brief Returns whether to use alpha transparency on newly created windows.

    \since 5.1
    \sa setDefaultAlphaBuffer()
 */
bool QQuickWindow::hasDefaultAlphaBuffer()
{
    return QQuickWindowPrivate::defaultAlphaBuffer;
}

/*!
    \brief \a useAlpha specifies whether to use alpha transparency on newly created windows.
    \since 5.1

    In any application which expects to create translucent windows, it's necessary to set
    this to true before creating the first QQuickWindow. The default value is false.

    \sa hasDefaultAlphaBuffer()
 */
void QQuickWindow::setDefaultAlphaBuffer(bool useAlpha)
{
    QQuickWindowPrivate::defaultAlphaBuffer = useAlpha;
}
#ifndef QT_NO_OPENGL
/*!
    \since 5.2

    Call this function to reset the OpenGL context its default state.

    The scene graph uses the OpenGL context and will both rely on and
    clobber its state. When mixing raw OpenGL commands with scene
    graph rendering, this function provides a convenient way of
    resetting the OpenGL context state back to its default values.

    This function does not touch state in the fixed-function pipeline.

    This function does not clear the color, depth and stencil buffers. Use
    QQuickWindow::setClearBeforeRendering to control clearing of the color
    buffer. The depth and stencil buffer might be clobbered by the scene
    graph renderer. Clear these manually on demand.

    \note This function only has an effect when using the default OpenGL scene graph
    adpation.

    \sa QQuickWindow::beforeRendering()
 */
void QQuickWindow::resetOpenGLState()
{
    if (!openglContext())
        return;

    Q_D(QQuickWindow);

    QOpenGLContext *ctx = openglContext();
    QOpenGLFunctions *gl = ctx->functions();

    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    if (!d->vaoHelper)
        d->vaoHelper = new QOpenGLVertexArrayObjectHelper(ctx);
    if (d->vaoHelper->isValid())
        d->vaoHelper->glBindVertexArray(0);

    if (ctx->isOpenGLES() || (gl->openGLFeatures() & QOpenGLFunctions::FixedFunctionPipeline)) {
        int maxAttribs;
        gl->glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
        for (int i=0; i<maxAttribs; ++i) {
            gl->glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, 0, 0);
            gl->glDisableVertexAttribArray(i);
        }
    }

    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, 0);

    gl->glDisable(GL_DEPTH_TEST);
    gl->glDisable(GL_STENCIL_TEST);
    gl->glDisable(GL_SCISSOR_TEST);

    gl->glColorMask(true, true, true, true);
    gl->glClearColor(0, 0, 0, 0);

    gl->glDepthMask(true);
    gl->glDepthFunc(GL_LESS);
    gl->glClearDepthf(1);

    gl->glStencilMask(0xff);
    gl->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    gl->glStencilFunc(GL_ALWAYS, 0, 0xff);

    gl->glDisable(GL_BLEND);
    gl->glBlendFunc(GL_ONE, GL_ZERO);

    gl->glUseProgram(0);

    QOpenGLFramebufferObject::bindDefault();
}
#endif
/*!
    \qmlproperty string Window::title

    The window's title in the windowing system.

    The window title might appear in the title area of the window decorations,
    depending on the windowing system and the window flags. It might also
    be used by the windowing system to identify the window in other contexts,
    such as in the task switcher.
 */

/*!
    \qmlproperty Qt::WindowModality Window::modality

    The modality of the window.

    A modal window prevents other windows from receiving input events.
    Possible values are Qt.NonModal (the default), Qt.WindowModal,
    and Qt.ApplicationModal.
 */

/*!
    \qmlproperty Qt::WindowFlags Window::flags

    The window flags of the window.

    The window flags control the window's appearance in the windowing system,
    whether it's a dialog, popup, or a regular window, and whether it should
    have a title bar, etc.

    The flags which you read from this property might differ from the ones
    that you set if the requested flags could not be fulfilled.
 */

/*!
    \qmlattachedproperty Window Window::window
    \since 5.7

    This attached property holds the item's window.
    The Window attached property can be attached to any Item.
*/

/*!
    \qmlattachedproperty int Window::width
    \qmlattachedproperty int Window::height
    \since 5.5

    These attached properties hold the size of the item's window.
    The Window attached property can be attached to any Item.
*/

/*!
    \qmlproperty int Window::x
    \qmlproperty int Window::y
    \qmlproperty int Window::width
    \qmlproperty int Window::height

    Defines the window's position and size.

    The (x,y) position is relative to the \l Screen if there is only one,
    or to the virtual desktop (arrangement of multiple screens).

    \qml
    Window { x: 100; y: 100; width: 100; height: 100 }
    \endqml

    \image screen-and-window-dimensions.jpg
 */

/*!
    \qmlproperty int Window::minimumWidth
    \qmlproperty int Window::minimumHeight
    \since 5.1

    Defines the window's minimum size.

    This is a hint to the window manager to prevent resizing below the specified
    width and height.
 */

/*!
    \qmlproperty int Window::maximumWidth
    \qmlproperty int Window::maximumHeight
    \since 5.1

    Defines the window's maximum size.

    This is a hint to the window manager to prevent resizing above the specified
    width and height.
 */

/*!
    \qmlproperty bool Window::visible

    Whether the window is visible on the screen.

    Setting visible to false is the same as setting \l visibility to \l {QWindow::}{Hidden}.

    \sa visibility
 */

/*!
    \qmlproperty QWindow::Visibility Window::visibility

    The screen-occupation state of the window.

    Visibility is whether the window should appear in the windowing system as
    normal, minimized, maximized, fullscreen or hidden.

    To set the visibility to \l {QWindow::}{AutomaticVisibility} means to give the
    window a default visible state, which might be \l {QWindow::}{FullScreen} or
    \l {QWindow::}{Windowed} depending on the platform. However when reading the
    visibility property you will always get the actual state, never
    \c AutomaticVisibility.

    When a window is not visible its visibility is Hidden, and setting
    visibility to \l {QWindow::}{Hidden} is the same as setting \l visible to \c false.

    \sa visible
    \since 5.1
 */

/*!
    \qmlattachedproperty QWindow::Visibility Window::visibility
    \since 5.4

    This attached property holds whether the window is currently shown
    in the windowing system as normal, minimized, maximized, fullscreen or
    hidden. The \c Window attached property can be attached to any Item. If the
    item is not shown in any window, the value will be \l {QWindow::}{Hidden}.

    \sa visible, visibility
*/

/*!
    \qmlproperty Item Window::contentItem
    \readonly
    \brief The invisible root item of the scene.
*/

/*!
    \qmlproperty Qt::ScreenOrientation Window::contentOrientation

    This is a hint to the window manager in case it needs to display
    additional content like popups, dialogs, status bars, or similar
    in relation to the window.

    The recommended orientation is \l {Screen::orientation}{Screen.orientation}, but
    an application doesn't have to support all possible orientations,
    and thus can opt to ignore the current screen orientation.

    The difference between the window and the content orientation
    determines how much to rotate the content by.

    The default value is Qt::PrimaryOrientation.

    \sa Screen

    \since 5.1
 */

/*!
    \qmlproperty real Window::opacity

    The opacity of the window.

    If the windowing system supports window opacity, this can be used to fade the
    window in and out, or to make it semitransparent.

    A value of 1.0 or above is treated as fully opaque, whereas a value of 0.0 or below
    is treated as fully transparent. Values inbetween represent varying levels of
    translucency between the two extremes.

    The default value is 1.0.

    \since 5.1
 */

/*!
    \qmlproperty Item Window::activeFocusItem
    \since 5.1

    The item which currently has active focus or \c null if there is
    no item with active focus.
 */

/*!
    \qmlattachedproperty Item Window::activeFocusItem
    \since 5.4

    This attached property holds the item which currently has active focus or
    \c null if there is no item with active focus. The Window attached property
    can be attached to any Item.
*/

/*!
    \qmlproperty bool Window::active
    \since 5.1

    The active status of the window.

    \sa requestActivate()
 */

/*!
    \qmlattachedproperty bool Window::active
    \since 5.4

    This attached property tells whether the window is active. The Window
    attached property can be attached to any Item.

    Here is an example which changes a label to show the active state of the
    window in which it is shown:

    \qml
    import QtQuick 2.4
    import QtQuick.Window 2.2

    Text {
        text: Window.active ? "active" : "inactive"
    }
    \endqml
*/

/*!
    \qmlmethod QtQuick::Window::requestActivate()
    \since 5.1

    Requests the window to be activated, i.e. receive keyboard focus.
 */

/*!
    \qmlmethod QtQuick::Window::alert(int msec)
    \since 5.1

    Causes an alert to be shown for \a msec milliseconds. If \a msec is \c 0
    (the default), then the alert is shown indefinitely until the window
    becomes active again.

    In alert state, the window indicates that it demands attention, for example
    by flashing or bouncing the taskbar entry.
*/

/*!
    \qmlmethod QtQuick::Window::close()

    Closes the window.

    When this method is called, or when the user tries to close the window by
    its title bar button, the \l closing signal will be emitted. If there is no
    handler, or the handler does not revoke permission to close, the window
    will subsequently close. If the QGuiApplication::quitOnLastWindowClosed
    property is \c true, and there are no other windows open, the application
    will quit.
*/

/*!
    \qmlmethod QtQuick::Window::raise()

    Raises the window in the windowing system.

    Requests that the window be raised to appear above other windows.
*/

/*!
    \qmlmethod QtQuick::Window::lower()

    Lowers the window in the windowing system.

    Requests that the window be lowered to appear below other windows.
*/

/*!
    \qmlmethod QtQuick::Window::show()

    Shows the window.

    This is equivalent to calling showFullScreen(), showMaximized(), or showNormal(),
    depending on the platform's default behavior for the window type and flags.

    \sa showFullScreen(), showMaximized(), showNormal(), hide(), QQuickItem::flags()
*/

/*!
    \qmlmethod QtQuick::Window::hide()

    Hides the window.

    Equivalent to setting \l visible to \c false or \l visibility to \l {QWindow::}{Hidden}.

    \sa show()
*/

/*!
    \qmlmethod QtQuick::Window::showMinimized()

    Shows the window as minimized.

    Equivalent to setting \l visibility to \l {QWindow::}{Minimized}.
*/

/*!
    \qmlmethod QtQuick::Window::showMaximized()

    Shows the window as maximized.

    Equivalent to setting \l visibility to \l {QWindow::}{Maximized}.
*/

/*!
    \qmlmethod QtQuick::Window::showFullScreen()

    Shows the window as fullscreen.

    Equivalent to setting \l visibility to \l {QWindow::}{FullScreen}.
*/

/*!
    \qmlmethod QtQuick::Window::showNormal()

    Shows the window as normal, i.e. neither maximized, minimized, nor fullscreen.

    Equivalent to setting \l visibility to \l {QWindow::}{Windowed}.
*/

/*!
    \enum QQuickWindow::RenderStage
    \since 5.4

    \value BeforeSynchronizingStage Before synchronization.
    \value AfterSynchronizingStage After synchronization.
    \value BeforeRenderingStage Before rendering.
    \value AfterRenderingStage After rendering.
    \value AfterSwapStage After the frame is swapped.
    \value NoStage As soon as possible. This value was added in Qt 5.6.

    \sa {Scene Graph and Rendering}
 */

/*!
    \since 5.4

    Schedules \a job to run when the rendering of this window reaches
    the given \a stage.

    This is a convenience to the equivalent signals in QQuickWindow for
    "one shot" tasks.

    The window takes ownership over \a job and will delete it when the
    job is completed.

    If rendering is shut down before \a job has a chance to run, the
    job will be run and then deleted as part of the scene graph cleanup.
    If the window is never shown and no rendering happens before the QQuickWindow
    is destroyed, all pending jobs will be destroyed without their run()
    method being called.

    If the rendering is happening on a different thread, then the job
    will happen on the rendering thread.

    If \a stage is \l NoStage, \a job will be run at the earliest opportunity
    whenever the render thread is not busy rendering a frame. If there is no
    OpenGL context available or the window is not exposed at the time the job is
    either posted or handled, it is deleted without executing the run() method.
    If a non-threaded renderer is in use, the run() method of the job is executed
    synchronously.
    The OpenGL context is changed to the renderer context before executing a
    \l NoStage job.

    \note This function does not trigger rendering; the jobs targeting any other
    stage than NoStage will be stored run until rendering is triggered elsewhere.
    To force the job to run earlier, call QQuickWindow::update();

    \sa beforeRendering(), afterRendering(), beforeSynchronizing(),
    afterSynchronizing(), frameSwapped(), sceneGraphInvalidated()
 */

void QQuickWindow::scheduleRenderJob(QRunnable *job, RenderStage stage)
{
    Q_D(QQuickWindow);

    d->renderJobMutex.lock();
    if (stage == BeforeSynchronizingStage) {
        d->beforeSynchronizingJobs << job;
    } else if (stage == AfterSynchronizingStage) {
        d->afterSynchronizingJobs << job;
    } else if (stage == BeforeRenderingStage) {
        d->beforeRenderingJobs << job;
    } else if (stage == AfterRenderingStage) {
        d->afterRenderingJobs << job;
    } else if (stage == AfterSwapStage) {
        d->afterSwapJobs << job;
    } else if (stage == NoStage) {
        if (isExposed())
            d->windowManager->postJob(this, job);
        else
            delete job;
    }
    d->renderJobMutex.unlock();
}

void QQuickWindowPrivate::runAndClearJobs(QList<QRunnable *> *jobs)
{
    renderJobMutex.lock();
    QList<QRunnable *> jobList = *jobs;
    jobs->clear();
    renderJobMutex.unlock();

    for (QRunnable *r : qAsConst(jobList)) {
        r->run();
        delete r;
    }
}

void QQuickWindow::runJobsAfterSwap()
{
    Q_D(QQuickWindow);
    d->runAndClearJobs(&d->afterSwapJobs);
}

/*!
 * Returns the device pixel ratio for this window.
 *
 * This is different from QWindow::devicePixelRatio() in that it supports
 * redirected rendering via QQuickRenderControl. When using a
 * QQuickRenderControl, the QQuickWindow is often not created, meaning it is
 * never shown and there is no underlying native window created in the
 * windowing system. As a result, querying properties like the device pixel
 * ratio cannot give correct results. Use this function instead.
 *
 * \sa QWindow::devicePixelRatio()
 */
qreal QQuickWindow::effectiveDevicePixelRatio() const
{
    QWindow *w = QQuickRenderControl::renderWindowFor(const_cast<QQuickWindow *>(this));
    return w ? w->devicePixelRatio() : devicePixelRatio();
}

/*!
    \return the current renderer interface. The value is always valid and is never null.

    \note This function can be called at any time after constructing the
    QQuickWindow, even while isSceneGraphInitialized() is still false. However,
    some renderer interface functions, in particular
    QSGRendererInterface::getResource() will not be functional until the
    scenegraph is up and running. Backend queries, like
    QSGRendererInterface::graphicsApi() or QSGRendererInterface::shaderType(),
    will always be functional on the other hand.

    \note The ownership of the returned pointer stays with Qt. The returned
    instance may or may not be shared between different QQuickWindow instances,
    depending on the scenegraph backend in use. Therefore applications are
    expected to query the interface object for each QQuickWindow instead of
    reusing the already queried pointer.

    \sa QSGRenderNode, QSGRendererInterface

    \since 5.8
 */
QSGRendererInterface *QQuickWindow::rendererInterface() const
{
    Q_D(const QQuickWindow);

    // no context validity check - it is essential to be able to return a
    // renderer interface instance before scenegraphInitialized() is emitted
    // (depending on the backend, that can happen way too late for some of the
    // rif use cases, like examining the graphics api or shading language in
    // use)

    return d->context->sceneGraphContext()->rendererInterface(d->context);
}

/*!
    Requests a Qt Quick scenegraph backend for the specified graphics \a api.
    Backends can either be built-in or be installed in form of dynamically
    loaded plugins.

    \note The call to the function must happen before constructing the first
    QQuickWindow in the application. It cannot be changed afterwards.

    If \a backend is invalid or an error occurs, the default backend (OpenGL or
    software, depending on the Qt configuration) is used.

    \since 5.8
 */
void QQuickWindow::setSceneGraphBackend(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Software:
        setSceneGraphBackend(QStringLiteral("software"));
        break;
    case QSGRendererInterface::Direct3D12:
        setSceneGraphBackend(QStringLiteral("d3d12"));
        break;
    default:
        break;
    }
}

/*!
    Requests the specified Qt Quick scenegraph \a backend. Backends can either
    be built-in or be installed in form of dynamically loaded plugins.

    \overload

    \note The call to the function must happen before constructing the first
    QQuickWindow in the application. It cannot be changed afterwards.

    If \a backend is invalid or an error occurs, the default backend (OpenGL or
    software, depending on the Qt configuration) is used.

    \note Calling this function is equivalent to setting the
    \c QT_QUICK_BACKEND or \c QMLSCENE_DEVICE environment variables. However, this
    API is safer to use in applications that spawn other processes as there is
    no need to worry about environment inheritance.

    \since 5.8
 */
void QQuickWindow::setSceneGraphBackend(const QString &backend)
{
    QSGContext::setBackend(backend);
}

/*!
    Creates a simple rectangle node. When the scenegraph is not initialized, the return value is null.

    This is cross-backend alternative to constructing a QSGSimpleRectNode directly.

    \since 5.8
    \sa QSGRectangleNode
 */
QSGRectangleNode *QQuickWindow::createRectangleNode() const
{
    Q_D(const QQuickWindow);
    return isSceneGraphInitialized() ? d->context->sceneGraphContext()->createRectangleNode() : nullptr;
}

/*!
    Creates a simple image node. When the scenegraph is not initialized, the return value is null.

    This is cross-backend alternative to constructing a QSGSimpleTextureNode directly.

    \since 5.8
    \sa QSGImageNode
 */
QSGImageNode *QQuickWindow::createImageNode() const
{
    Q_D(const QQuickWindow);
    return isSceneGraphInitialized() ? d->context->sceneGraphContext()->createImageNode() : nullptr;
}

/*!
    Creates a nine patch node. When the scenegraph is not initialized, the return value is null.

    \since 5.8
 */
QSGNinePatchNode *QQuickWindow::createNinePatchNode() const
{
    Q_D(const QQuickWindow);
    return isSceneGraphInitialized() ? d->context->sceneGraphContext()->createNinePatchNode() : nullptr;
}

#include "moc_qquickwindow.cpp"

QT_END_NAMESPACE
