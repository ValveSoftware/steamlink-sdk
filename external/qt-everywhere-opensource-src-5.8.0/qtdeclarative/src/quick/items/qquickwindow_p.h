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

#ifndef QQUICKWINDOW_P_H
#define QQUICKWINDOW_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qquickitem.h"
#include "qquickwindow.h"
#include "qquickevents_p_p.h"

#include <QtQuick/private/qsgcontext_p.h>

#include <QtCore/qthread.h>
#include <QtCore/qmutex.h>
#include <QtCore/qwaitcondition.h>
#include <QtCore/qrunnable.h>
#include <private/qwindow_p.h>
#include <private/qopengl_p.h>
#include <qopenglcontext.h>
#include <QtGui/qopenglframebufferobject.h>
#include <QtGui/qevent.h>

QT_BEGIN_NAMESPACE

class QOpenGLVertexArrayObjectHelper;
class QQuickAnimatorController;
class QQuickDragGrabber;
class QQuickItemPrivate;
class QQuickPointerDevice;
class QQuickRenderControl;
class QQuickWindowIncubationController;
class QQuickWindowPrivate;
class QQuickWindowRenderLoop;
class QSGRenderLoop;
class QTouchEvent;

//Make it easy to identify and customize the root item if needed
class QQuickRootItem : public QQuickItem
{
    Q_OBJECT
public:
    QQuickRootItem();
public Q_SLOTS:
    void setWidth(int w) {QQuickItem::setWidth(qreal(w));}
    void setHeight(int h) {QQuickItem::setHeight(qreal(h));}
};

class Q_QUICK_PRIVATE_EXPORT QQuickCustomRenderStage
{
public:
    virtual ~QQuickCustomRenderStage() {}
    virtual bool render() = 0;
    virtual bool swap() = 0;
};

class Q_QUICK_PRIVATE_EXPORT QQuickWindowPrivate : public QWindowPrivate
{
public:
    Q_DECLARE_PUBLIC(QQuickWindow)

    enum CustomEvents {
        FullUpdateRequest = QEvent::User + 1
    };

    static inline QQuickWindowPrivate *get(QQuickWindow *c) { return c->d_func(); }

    QQuickWindowPrivate();
    virtual ~QQuickWindowPrivate();

    void init(QQuickWindow *, QQuickRenderControl *control = 0);

    QQuickRootItem *contentItem;
    QSet<QQuickItem *> parentlessItems;
    QQmlListProperty<QObject> data();

    QQuickItem *activeFocusItem;

    void deliverKeyEvent(QKeyEvent *e);

    // Keeps track of the item currently receiving mouse events
#ifndef QT_NO_CURSOR
    QQuickItem *cursorItem;
#endif
#ifndef QT_NO_DRAGANDDROP
    QQuickDragGrabber *dragGrabber;
#endif
    int touchMouseId;
    QQuickPointerDevice *touchMouseDevice;
    bool checkIfDoubleClicked(ulong newPressEventTimestamp);
    ulong touchMousePressTimestamp;

    // Mouse positions are saved in widget coordinates
    QPointF lastMousePosition;
    bool deliverTouchAsMouse(QQuickItem *item, QQuickPointerEvent *pointerEvent);
    void translateTouchEvent(QTouchEvent *touchEvent);
    void setMouseGrabber(QQuickItem *grabber);
    void grabTouchPoints(QQuickItem *grabber, const QVector<int> &ids);
    void removeGrabber(QQuickItem *grabber, bool mouse = true, bool touch = true);
    static QMouseEvent *cloneMouseEvent(QMouseEvent *event, QPointF *transformedLocalPos = 0);
    void deliverMouseEvent(QQuickPointerMouseEvent *pointerEvent);
    bool sendFilteredMouseEvent(QQuickItem *, QQuickItem *, QEvent *, QSet<QQuickItem *> *);
#ifndef QT_NO_WHEELEVENT
    bool deliverWheelEvent(QQuickItem *, QWheelEvent *);
#endif
#ifndef QT_NO_GESTURES
    bool deliverNativeGestureEvent(QQuickItem *, QNativeGestureEvent *);
#endif

    // entry point of events to the window
    void handleTouchEvent(QTouchEvent *);
    void handleMouseEvent(QMouseEvent *);
    bool compressTouchEvent(QTouchEvent *);
    void flushFrameSynchronousEvents();
    void deliverDelayedTouchEvent();

    // delivery of pointer events:
    QQuickPointerEvent *pointerEventInstance(QEvent *ev);
    void deliverPointerEvent(QQuickPointerEvent *);
    void deliverTouchEvent(QQuickPointerTouchEvent *);
    bool deliverTouchCancelEvent(QTouchEvent *);
    bool deliverPressEvent(QQuickPointerEvent *, QSet<QQuickItem *> *);
    bool deliverUpdatedTouchPoints(QQuickPointerTouchEvent *event, QSet<QQuickItem *> *hasFiltered);
    bool deliverMatchingPointsToItem(QQuickItem *item, QQuickPointerEvent *pointerEvent, QSet<QQuickItem*> *filtered);
    bool sendFilteredTouchEvent(QQuickItem *target, QQuickItem *item, QQuickPointerTouchEvent *event, QSet<QQuickItem*> *filtered);

    QVector<QQuickItem *> pointerTargets(QQuickItem *, const QPointF &, bool checkMouseButtons) const;
    QVector<QQuickItem *> mergePointerTargets(const QVector<QQuickItem *> &list1, const QVector<QQuickItem *> &list2) const;

    // hover delivery
    bool deliverHoverEvent(QQuickItem *, const QPointF &scenePos, const QPointF &lastScenePos, Qt::KeyboardModifiers modifiers, ulong timestamp, bool &accepted);
    bool sendHoverEvent(QEvent::Type, QQuickItem *, const QPointF &scenePos, const QPointF &lastScenePos,
                        Qt::KeyboardModifiers modifiers, ulong timestamp, bool accepted);
    bool clearHover(ulong timestamp = 0);

#ifndef QT_NO_DRAGANDDROP
    void deliverDragEvent(QQuickDragGrabber *, QEvent *);
    bool deliverDragEvent(QQuickDragGrabber *, QQuickItem *, QDragMoveEvent *);
#endif
#ifndef QT_NO_CURSOR
    void updateCursor(const QPointF &scenePos);
    QQuickItem *findCursorItem(QQuickItem *item, const QPointF &scenePos);
#endif

    QList<QQuickItem*> hoverItems;
    enum FocusOption {
        DontChangeFocusProperty = 0x01,
        DontChangeSubFocusItem  = 0x02
    };
    Q_DECLARE_FLAGS(FocusOptions, FocusOption)

    void setFocusInScope(QQuickItem *scope, QQuickItem *item, Qt::FocusReason reason, FocusOptions = 0);
    void clearFocusInScope(QQuickItem *scope, QQuickItem *item, Qt::FocusReason reason, FocusOptions = 0);
    static void notifyFocusChangesRecur(QQuickItem **item, int remaining);
    void clearFocusObject();

    void updateFocusItemTransform();

    void dirtyItem(QQuickItem *);
    void cleanup(QSGNode *);

    void polishItems();
    void forcePolish();
    void syncSceneGraph();
    void renderSceneGraph(const QSize &size);

    bool isRenderable() const;

    bool emitError(QQuickWindow::SceneGraphError error, const QString &msg);

    QQuickItem::UpdatePaintNodeData updatePaintNodeData;

    QQuickItem *dirtyItemList;
    QList<QSGNode *> cleanupNodeList;

    QVector<QQuickItem *> itemsToPolish;

    qreal devicePixelRatio;
    QMetaObject::Connection physicalDpiChangedConnection;

    void updateDirtyNodes();
    void cleanupNodes();
    void cleanupNodesOnShutdown();
    bool updateEffectiveOpacity(QQuickItem *);
    void updateEffectiveOpacityRoot(QQuickItem *, qreal);
    void updateDirtyNode(QQuickItem *);

    void fireFrameSwapped() { Q_EMIT q_func()->frameSwapped(); }
    void fireOpenGLContextCreated(QOpenGLContext *context) { Q_EMIT q_func()->openglContextCreated(context); }
    void fireAboutToStop() { Q_EMIT q_func()->sceneGraphAboutToStop(); }

    QSGRenderContext *context;
    QSGRenderer *renderer;
    QByteArray customRenderMode; // Default renderer supports "clip", "overdraw", "changes", "batches" and blank.

    QSGRenderLoop *windowManager;
    QQuickRenderControl *renderControl;
    QQuickAnimatorController *animationController;
    QScopedPointer<QTouchEvent> delayedTouch;

    int pointerEventRecursionGuard;
    QQuickCustomRenderStage *customRenderStage;

    QColor clearColor;

    uint clearBeforeRendering : 1;

    uint persistentGLContext : 1;
    uint persistentSceneGraph : 1;

    uint lastWheelEventAccepted : 1;
    bool componentCompleted : 1;

    Qt::FocusReason lastFocusReason;

    QOpenGLFramebufferObject *renderTarget;
    uint renderTargetId;
    QSize renderTargetSize;

    QOpenGLVertexArrayObjectHelper *vaoHelper;

    mutable QQuickWindowIncubationController *incubationController;

    static bool defaultAlphaBuffer;

    static bool dragOverThreshold(qreal d, Qt::Axis axis, QMouseEvent *event, int startDragThreshold = -1);
    static bool dragOverThreshold(qreal d, Qt::Axis axis, const QTouchEvent::TouchPoint *tp, int startDragThreshold = -1);

    // data property
    static void data_append(QQmlListProperty<QObject> *, QObject *);
    static int data_count(QQmlListProperty<QObject> *);
    static QObject *data_at(QQmlListProperty<QObject> *, int);
    static void data_clear(QQmlListProperty<QObject> *);

    static void contextCreationFailureMessage(const QSurfaceFormat &format,
                                              QString *translatedMessage,
                                              QString *untranslatedMessage,
                                              bool isEs);

    QMutex renderJobMutex;
    QList<QRunnable *> beforeSynchronizingJobs;
    QList<QRunnable *> afterSynchronizingJobs;
    QList<QRunnable *> beforeRenderingJobs;
    QList<QRunnable *> afterRenderingJobs;
    QList<QRunnable *> afterSwapJobs;

    void runAndClearJobs(QList<QRunnable *> *jobs);

private:
    static void cleanupNodesOnShutdown(QQuickItem *);
};

class QQuickWindowQObjectCleanupJob : public QRunnable
{
public:
    QQuickWindowQObjectCleanupJob(QObject *o) : object(o) { }
    void run() Q_DECL_OVERRIDE { delete object; }
    QObject *object;
    static void schedule(QQuickWindow *window, QObject *object) {
        Q_ASSERT(window);
        Q_ASSERT(object);
        window->scheduleRenderJob(new QQuickWindowQObjectCleanupJob(object), QQuickWindow::AfterSynchronizingStage);
    }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickWindowPrivate::FocusOptions)

QT_END_NAMESPACE

#endif // QQUICKWINDOW_P_H
