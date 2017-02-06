/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickdrawer_p.h"
#include "qquickdrawer_p_p.h"

#include <QtGui/qstylehints.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qquickanimation_p.h>
#include <QtQuick/private/qquicktransition_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Drawer
    \inherits Popup
    \instantiates QQuickDrawer
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-navigation
    \ingroup qtquickcontrols2-popups
    \brief Side panel that can be opened and closed using a swipe gesture.

    Drawer provides a swipe-based side panel, similar to those often used in
    touch interfaces to provide a central location for navigation.

    \image qtquickcontrols2-drawer.gif

    Drawer can be positioned at any of the four edges of the content item.
    The drawer above is positioned against the left edge of the window. The
    drawer is then opened by \e "dragging" it out from the left edge of the
    window.

    \code
    import QtQuick 2.7
    import QtQuick.Controls 2.0

    ApplicationWindow {
        id: window
        visible: true

        Drawer {
            id: drawer
            width: 0.66 * window.width
            height: window.height
        }
    }
    \endcode

    Drawer is a special type of popup that resides at one of the window \l {edge}{edges}.
    By default, Drawer re-parents itself to the window \l {ApplicationWindow::}{overlay},
    and therefore operates on window coordinates. It is also possible to manually set the
    \l {Popup::}{parent} to something else to make the drawer operate in a specific
    coordinate space.

    Drawer can be configured to cover only part of its window edge. The following example
    illustrates how Drawer can be positioned to appear below a window header:

    \code
    import QtQuick 2.7
    import QtQuick.Controls 2.0

    ApplicationWindow {
        id: window
        visible: true

        header: ToolBar { }

        Drawer {
            y: header.height
            width: window.width * 0.6
            height: window.height - header.height
        }
    }
    \endcode

    The \l position property determines how much of the drawer is visible, as
    a value between \c 0.0 and \c 1.0. It is not possible to set the x-coordinate
    (or horizontal margins) of a drawer at the left or right window edge, or the
    y-coordinate (or vertical margins) of a drawer at the top or bottom window edge.

    In the image above, the application's contents are \e "pushed" across the
    screen. This is achieved by applying a translation to the contents:

    \code
    import QtQuick 2.7
    import QtQuick.Controls 2.1

    ApplicationWindow {
        id: window
        width: 200
        height: 228
        visible: true

        Drawer {
            id: drawer
            width: 0.66 * window.width
            height: window.height
        }

        Label {
            id: content

            text: "Aa"
            font.pixelSize: 96
            anchors.fill: parent
            verticalAlignment: Label.AlignVCenter
            horizontalAlignment: Label.AlignHCenter

            transform: Translate {
                x: drawer.position * content.width * 0.33
            }
        }
    }
    \endcode

    If you would like the application's contents to stay where they are when
    the drawer is opened, don't apply a translation.

    \note On some platforms, certain edges may be reserved for system
    gestures and therefore cannot be used with Drawer. For example, the
    top and bottom edges may be reserved for system notifications and
    control centers on Android and iOS.

    \sa SwipeView, {Customizing Drawer}, {Navigation Controls}, {Popup Controls}
*/

QQuickDrawerPrivate::QQuickDrawerPrivate()
    : edge(Qt::LeftEdge), offset(0), position(0),
      dragMargin(QGuiApplication::styleHints()->startDragDistance())
{
    setEdge(Qt::LeftEdge);
}

qreal QQuickDrawerPrivate::positionAt(const QPointF &point) const
{
    Q_Q(const QQuickDrawer);
    QQuickWindow *window = q->window();
    if (!window)
        return 0;

    switch (edge) {
    case Qt::TopEdge:
        return point.y() / q->height();
    case Qt::LeftEdge:
        return point.x() / q->width();
    case Qt::RightEdge:
        return (window->width() - point.x()) / q->width();
    case Qt::BottomEdge:
        return (window->height() - point.y()) / q->height();
    default:
        return 0;
    }
}

void QQuickDrawerPrivate::reposition()
{
    Q_Q(QQuickDrawer);
    QQuickWindow *window = q->window();
    if (!window)
        return;

    switch (edge) {
    case Qt::LeftEdge:
        popupItem->setX((position - 1.0) * popupItem->width());
        break;
    case Qt::RightEdge:
        popupItem->setX(window->width() - position * popupItem->width());
        break;
    case Qt::TopEdge:
        popupItem->setY((position - 1.0) * popupItem->height());
        break;
    case Qt::BottomEdge:
        popupItem->setY(window->height() - position * popupItem->height());
        break;
    }

    QQuickPopupPrivate::reposition();
}

void QQuickDrawerPrivate::resizeOverlay()
{
    if (!dimmer || !window)
        return;

    QRectF geometry(0, 0, window->width(), window->height());

    if (edge == Qt::LeftEdge || edge == Qt::RightEdge) {
        geometry.setY(popupItem->y());
        geometry.setHeight(popupItem->height());
    } else {
        geometry.setX(popupItem->x());
        geometry.setWidth(popupItem->width());
    }

    dimmer->setPosition(geometry.topLeft());
    dimmer->setSize(geometry.size());
}

static bool dragOverThreshold(qreal d, Qt::Axis axis, QMouseEvent *event, int threshold = -1)
{
    return QQuickWindowPrivate::dragOverThreshold(d, axis, event, threshold);
}

bool QQuickDrawerPrivate::startDrag(QQuickWindow *window, QMouseEvent *event)
{
    if (!window || dragMargin < 0.0 || qFuzzyIsNull(dragMargin))
        return false;

    bool drag = false;
    switch (edge) {
    case Qt::LeftEdge:
        drag = !dragOverThreshold(event->windowPos().x(), Qt::XAxis, event, dragMargin);
        break;
    case Qt::RightEdge:
        drag = !dragOverThreshold(window->width() - event->windowPos().x(), Qt::XAxis, event, dragMargin);
        break;
    case Qt::TopEdge:
        drag = !dragOverThreshold(event->windowPos().y(), Qt::YAxis, event, dragMargin);
        break;
    case Qt::BottomEdge:
        drag = !dragOverThreshold(window->height() - event->windowPos().y(), Qt::YAxis, event, dragMargin);
        break;
    default:
        break;
    }

    if (drag) {
        prepareEnterTransition();
        reposition();
        handleMousePressEvent(window->contentItem(), event);
    }

    return drag;
}

bool QQuickDrawerPrivate::grabMouse(QMouseEvent *event)
{
    Q_Q(QQuickDrawer);
    if (!window || popupItem->keepMouseGrab())
        return false;

    const QPointF movePoint = event->windowPos();

    // Flickable uses a hard-coded threshold of 15 for flicking, and
    // QStyleHints::startDragDistance for dragging. Drawer uses a bit
    // larger threshold to avoid being too eager to steal touch (QTBUG-50045)
    const int threshold = qMax(20, QGuiApplication::styleHints()->startDragDistance() + 5);
    bool overThreshold = false;
    if (position > 0 || dragMargin > 0) {
        if (edge == Qt::LeftEdge || edge == Qt::RightEdge)
            overThreshold = dragOverThreshold(movePoint.x() - pressPoint.x(), Qt::XAxis, event, threshold);
        else
            overThreshold = dragOverThreshold(movePoint.y() - pressPoint.y(), Qt::YAxis, event, threshold);
    }

    // Don't be too eager to steal presses outside the drawer (QTBUG-53929)
    if (overThreshold && qFuzzyCompare(position, qreal(1.0)) && !popupItem->contains(popupItem->mapFromScene(movePoint))) {
        if (edge == Qt::LeftEdge || edge == Qt::RightEdge)
            overThreshold = qAbs(movePoint.x() - q->width()) < dragMargin;
        else
            overThreshold = qAbs(movePoint.y() - q->height()) < dragMargin;
    }

    return overThreshold;
}

static const qreal openCloseVelocityThreshold = 300;

bool QQuickDrawerPrivate::ungrabMouse(QMouseEvent *event)
{
    bool wasGrabbed = popupItem->keepMouseGrab();
    if (wasGrabbed) {
        const QPointF releasePoint = event->windowPos();
        velocityCalculator.stopMeasuring(releasePoint, event->timestamp());

        qreal velocity = 0;
        if (edge == Qt::LeftEdge || edge == Qt::RightEdge)
            velocity = velocityCalculator.velocity().x();
        else
            velocity = velocityCalculator.velocity().y();

        // the velocity is calculated so that swipes from left to right
        // and top to bottom have positive velocity, and swipes from right
        // to left and bottom to top have negative velocity.
        //
        // - top/left edge: positive velocity opens, negative velocity closes
        // - bottom/right edge: negative velocity opens, positive velocity closes
        //
        // => invert the velocity for bottom and right edges, for the threshold comparison below
        if (edge == Qt::RightEdge || edge == Qt::BottomEdge)
            velocity = -velocity;

        if (position > 0.7 || velocity > openCloseVelocityThreshold) {
            transitionManager.transitionEnter();
        } else if (position < 0.3 || velocity < -openCloseVelocityThreshold) {
            transitionManager.transitionExit();
        } else {
            switch (edge) {
            case Qt::LeftEdge:
                if (releasePoint.x() - pressPoint.x() > 0)
                    transitionManager.transitionEnter();
                else
                    transitionManager.transitionExit();
                break;
            case Qt::RightEdge:
                if (releasePoint.x() - pressPoint.x() < 0)
                    transitionManager.transitionEnter();
                else
                    transitionManager.transitionExit();
                break;
            case Qt::TopEdge:
                if (releasePoint.y() - pressPoint.y() > 0)
                    transitionManager.transitionEnter();
                else
                    transitionManager.transitionExit();
                break;
            case Qt::BottomEdge:
                if (releasePoint.y() - pressPoint.y() < 0)
                    transitionManager.transitionEnter();
                else
                    transitionManager.transitionExit();
                break;
            }
        }
    }
    return wasGrabbed;
}

bool QQuickDrawerPrivate::handleMousePressEvent(QQuickItem *item, QMouseEvent *event)
{
    offset = 0;
    pressPoint = event->windowPos();
    velocityCalculator.startMeasuring(pressPoint, event->timestamp());

    // don't block press events
    // a) outside a non-modal drawer,
    // b) to drawer children, or
    // c) outside a modal drawer's background dimming
    event->setAccepted(modal && !popupItem->isAncestorOf(item) && (!dimmer || dimmer->contains(dimmer->mapFromScene(pressPoint))));
    return event->isAccepted();
}

bool QQuickDrawerPrivate::handleMouseMoveEvent(QQuickItem *item, QMouseEvent *event)
{
    Q_Q(QQuickDrawer);
    Q_UNUSED(item);

    // Don't react to synthesized mouse move events at INF,INF coordinates.
    // QQuickWindowPrivate::translateTouchToMouse() uses them to clear hover
    // on touch release (QTBUG-55995).
    if (qIsInf(event->screenPos().x()) || qIsInf(event->screenPos().y()))
        return true;

    const QPointF movePoint = event->windowPos();

    if (grabMouse(event)) {
        QQuickItem *grabber = window->mouseGrabberItem();
        if (!grabber || !grabber->keepMouseGrab()) {
            popupItem->grabMouse();
            popupItem->setKeepMouseGrab(true);
            offset = qMin<qreal>(0.0, positionAt(movePoint) - position);
        }
    }

    if (popupItem->keepMouseGrab())
        q->setPosition(positionAt(movePoint) - offset);
    event->accept();

    return popupItem->keepMouseGrab();
}

bool QQuickDrawerPrivate::handleMouseReleaseEvent(QQuickItem *item, QMouseEvent *event)
{
    Q_UNUSED(item);

    const bool wasGrabbed = ungrabMouse(event);

    popupItem->setKeepMouseGrab(false);
    pressPoint = QPoint();
    event->accept();

    return wasGrabbed;
}

static QList<QQuickStateAction> prepareTransition(QQuickDrawer *drawer, QQuickTransition *transition, qreal to)
{
    QList<QQuickStateAction> actions;
    if (!transition || !QQuickPopupPrivate::get(drawer)->window || !transition->enabled())
        return actions;

    qmlExecuteDeferred(transition);

    QQmlProperty defaultTarget(drawer, QLatin1String("position"));
    QQmlListProperty<QQuickAbstractAnimation> animations = transition->animations();
    int count = animations.count(&animations);
    for (int i = 0; i < count; ++i) {
        QQuickAbstractAnimation *anim = animations.at(&animations, i);
        anim->setDefaultTarget(defaultTarget);
    }

    actions << QQuickStateAction(drawer, QLatin1String("position"), to);
    return actions;
}

bool QQuickDrawerPrivate::prepareEnterTransition()
{
    Q_Q(QQuickDrawer);
    enterActions = prepareTransition(q, enter, 1.0);
    return QQuickPopupPrivate::prepareEnterTransition();
}

bool QQuickDrawerPrivate::prepareExitTransition()
{
    Q_Q(QQuickDrawer);
    exitActions = prepareTransition(q, exit, 0.0);
    return QQuickPopupPrivate::prepareExitTransition();
}

void QQuickDrawerPrivate::setEdge(Qt::Edge e)
{
    edge = e;
    if (edge == Qt::LeftEdge || edge == Qt::RightEdge) {
        allowVerticalMove = true;
        allowVerticalResize = true;
        allowHorizontalMove = false;
        allowHorizontalResize = false;
    } else {
        allowVerticalMove = false;
        allowVerticalResize = false;
        allowHorizontalMove = true;
        allowHorizontalResize = true;
    }
}

QQuickDrawer::QQuickDrawer(QObject *parent) :
    QQuickPopup(*(new QQuickDrawerPrivate), parent)
{
    setFocus(true);
    setModal(true);
    setFiltersChildMouseEvents(true);
    setClosePolicy(CloseOnEscape | CloseOnReleaseOutside);
}

/*!
    \qmlproperty enumeration QtQuick.Controls::Drawer::edge

    This property holds the edge of the window at which the drawer will
    open from. The acceptable values are:

    \value Qt.TopEdge     The top edge of the window.
    \value Qt.LeftEdge    The left edge of the window (default).
    \value Qt.RightEdge   The right edge of the window.
    \value Qt.BottomEdge  The bottom edge of the window.
*/
Qt::Edge QQuickDrawer::edge() const
{
    Q_D(const QQuickDrawer);
    return d->edge;
}

void QQuickDrawer::setEdge(Qt::Edge edge)
{
    Q_D(QQuickDrawer);
    if (d->edge == edge)
        return;

    d->setEdge(edge);
    if (isComponentComplete())
        d->reposition();
    emit edgeChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Drawer::position

    This property holds the position of the drawer relative to its final
    destination. That is, the position will be \c 0.0 when the drawer
    is fully closed, and \c 1.0 when fully open.
*/
qreal QQuickDrawer::position() const
{
    Q_D(const QQuickDrawer);
    return d->position;
}

void QQuickDrawer::setPosition(qreal position)
{
    Q_D(QQuickDrawer);
    position = qBound<qreal>(0.0, position, 1.0);
    if (qFuzzyCompare(d->position, position))
        return;

    d->position = position;
    if (isComponentComplete())
        d->reposition();
    if (d->dimmer)
        d->dimmer->setOpacity(position);
    emit positionChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Drawer::dragMargin

    This property holds the distance from the screen edge within which
    drag actions will open the drawer. Setting the value to \c 0 or less
    prevents opening the drawer by dragging.

    The default value is \c Qt.styleHints.startDragDistance.
*/
qreal QQuickDrawer::dragMargin() const
{
    Q_D(const QQuickDrawer);
    return d->dragMargin;
}

void QQuickDrawer::setDragMargin(qreal margin)
{
    Q_D(QQuickDrawer);
    if (qFuzzyCompare(d->dragMargin, margin))
        return;

    d->dragMargin = margin;
    emit dragMarginChanged();
}

void QQuickDrawer::resetDragMargin()
{
    setDragMargin(QGuiApplication::styleHints()->startDragDistance());
}

bool QQuickDrawer::childMouseEventFilter(QQuickItem *child, QEvent *event)
{
    Q_D(QQuickDrawer);
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        return d->handleMousePressEvent(child, static_cast<QMouseEvent *>(event));
    case QEvent::MouseMove:
        return d->handleMouseMoveEvent(child, static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonRelease:
        return d->handleMouseReleaseEvent(child, static_cast<QMouseEvent *>(event));
    default:
        return false;
    }
}

void QQuickDrawer::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickDrawer);
    QQuickPopup::mousePressEvent(event);
    d->handleMousePressEvent(d->popupItem, event);
}

void QQuickDrawer::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickDrawer);
    QQuickPopup::mouseMoveEvent(event);
    d->handleMouseMoveEvent(d->popupItem, event);
}

void QQuickDrawer::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickDrawer);
    QQuickPopup::mouseReleaseEvent(event);
    d->handleMouseReleaseEvent(d->popupItem, event);
}

void QQuickDrawer::mouseUngrabEvent()
{
    Q_D(QQuickDrawer);
    QQuickPopup::mouseUngrabEvent();
    d->pressPoint = QPoint();
    d->velocityCalculator.reset();
}

bool QQuickDrawer::overlayEvent(QQuickItem *item, QEvent *event)
{
    Q_D(QQuickDrawer);
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        d->tryClose(item, static_cast<QMouseEvent *>(event));
        return d->handleMousePressEvent(item, static_cast<QMouseEvent *>(event));
    case QEvent::MouseMove:
        return d->handleMouseMoveEvent(item, static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonRelease:
        d->tryClose(item, static_cast<QMouseEvent *>(event));
        return d->handleMouseReleaseEvent(item, static_cast<QMouseEvent *>(event));
    default:
        return QQuickPopup::overlayEvent(item, event);
    }
}

void QQuickDrawer::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickDrawer);
    QQuickPopup::geometryChanged(newGeometry, oldGeometry);
    d->resizeOverlay();
}

QT_END_NAMESPACE
