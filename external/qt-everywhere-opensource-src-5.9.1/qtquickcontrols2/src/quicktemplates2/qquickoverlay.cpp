/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qquickoverlay_p.h"
#include "qquickoverlay_p_p.h"
#include "qquickpopupitem_p_p.h"
#include "qquickpopup_p_p.h"
#include "qquickdrawer_p_p.h"
#include "qquickapplicationwindow_p.h"
#include <QtQml/qqmlinfo.h>
#include <QtQml/qqmlproperty.h>
#include <QtQml/qqmlcomponent.h>
#include <algorithm>

QT_BEGIN_NAMESPACE

void QQuickOverlayPrivate::popupAboutToShow()
{
    Q_Q(QQuickOverlay);
    QQuickPopup *popup = qobject_cast<QQuickPopup *>(q->sender());
    if (!popup || !popup->dim())
        return;

    // use QQmlProperty instead of QQuickItem::setOpacity() to trigger QML Behaviors
    QQuickPopupPrivate *p = QQuickPopupPrivate::get(popup);
    if (p->dimmer)
        QQmlProperty::write(p->dimmer, QStringLiteral("opacity"), 1.0);
}

void QQuickOverlayPrivate::popupAboutToHide()
{
    Q_Q(QQuickOverlay);
    QQuickPopup *popup = qobject_cast<QQuickPopup *>(q->sender());
    if (!popup || !popup->dim())
        return;

    // use QQmlProperty instead of QQuickItem::setOpacity() to trigger QML Behaviors
    QQuickPopupPrivate *p = QQuickPopupPrivate::get(popup);
    if (p->dimmer)
        QQmlProperty::write(p->dimmer, QStringLiteral("opacity"), 0.0);
}

static QQuickItem *createDimmer(QQmlComponent *component, QQuickPopup *popup, QQuickItem *parent)
{
    QQuickItem *item = nullptr;
    if (component) {
        QQmlContext *creationContext = component->creationContext();
        if (!creationContext)
            creationContext = qmlContext(parent);
        QQmlContext *context = new QQmlContext(creationContext, parent);
        context->setContextObject(popup);
        item = qobject_cast<QQuickItem*>(component->beginCreate(context));
    }

    // when there is no overlay component available (with plain QQuickWindow),
    // use a plain QQuickItem as a fallback to block hover events
    if (!item && popup->isModal())
        item = new QQuickItem;

    if (item) {
        item->setOpacity(popup->isVisible() ? 1.0 : 0.0);
        item->setParentItem(parent);
        item->stackBefore(popup->popupItem());
        item->setZ(popup->z());
        if (popup->isModal()) {
            item->setAcceptedMouseButtons(Qt::AllButtons);
#if QT_CONFIG(cursor)
            item->setCursor(Qt::ArrowCursor);
#endif
#if QT_CONFIG(quicktemplates2_hover)
            // TODO: switch to QStyleHints::useHoverEffects in Qt 5.8
            item->setAcceptHoverEvents(true);
            // item->setAcceptHoverEvents(QGuiApplication::styleHints()->useHoverEffects());
            // connect(QGuiApplication::styleHints(), &QStyleHints::useHoverEffectsChanged, item, &QQuickItem::setAcceptHoverEvents);
#endif
        }
        if (component)
            component->completeCreate();
    }
    return item;
}

void QQuickOverlayPrivate::createOverlay(QQuickPopup *popup)
{
    Q_Q(QQuickOverlay);
    QQuickPopupPrivate *p = QQuickPopupPrivate::get(popup);
    if (!p->dimmer)
        p->dimmer = createDimmer(popup->isModal() ? modal : modeless, popup, q);
    p->resizeOverlay();
}

void QQuickOverlayPrivate::destroyOverlay(QQuickPopup *popup)
{
    QQuickPopupPrivate *p = QQuickPopupPrivate::get(popup);
    if (p->dimmer) {
        p->dimmer->setParentItem(nullptr);
        p->dimmer->deleteLater();
        p->dimmer = nullptr;
    }
}

void QQuickOverlayPrivate::toggleOverlay()
{
    Q_Q(QQuickOverlay);
    QQuickPopup *popup = qobject_cast<QQuickPopup *>(q->sender());
    if (!popup)
        return;

    destroyOverlay(popup);
    if (popup->dim())
        createOverlay(popup);
}

QVector<QQuickPopup *> QQuickOverlayPrivate::stackingOrderPopups() const
{
    const QList<QQuickItem *> children = paintOrderChildItems();

    QVector<QQuickPopup *> popups;
    popups.reserve(children.count());

    for (auto it = children.crbegin(), end = children.crend(); it != end; ++it) {
        QQuickPopup *popup = qobject_cast<QQuickPopup *>((*it)->parent());
        if (popup)
            popups += popup;
    }

    return popups;
}

QVector<QQuickDrawer *> QQuickOverlayPrivate::stackingOrderDrawers() const
{
    QVector<QQuickDrawer *> sorted(allDrawers);
    std::sort(sorted.begin(), sorted.end(), [](const QQuickDrawer *one, const QQuickDrawer *another) {
        return one->z() > another->z();
    });
    return sorted;
}

void QQuickOverlayPrivate::itemGeometryChanged(QQuickItem *item, QQuickGeometryChange, const QRectF &)
{
    Q_Q(QQuickOverlay);
    q->setSize(QSizeF(item->width(), item->height()));
}

QQuickOverlayPrivate::QQuickOverlayPrivate()
    : modal(nullptr),
      modeless(nullptr)
{
}

bool QQuickOverlayPrivate::startDrag(QEvent *event, const QPointF &pos)
{
    Q_Q(QQuickOverlay);
    if (allDrawers.isEmpty())
        return false;

    // don't start dragging a drawer if a modal popup overlay is blocking (QTBUG-60602)
    QQuickItem *item = q->childAt(pos.x(), pos.y());
    if (item) {
        const auto popups = stackingOrderPopups();
        for (QQuickPopup *popup : popups) {
            QQuickPopupPrivate *p = QQuickPopupPrivate::get(popup);
            if (p->dimmer == item && popup->isVisible() && popup->isModal())
                return false;
        }
    }

    const QVector<QQuickDrawer *> drawers = stackingOrderDrawers();
    for (QQuickDrawer *drawer : drawers) {
        QQuickDrawerPrivate *p = QQuickDrawerPrivate::get(drawer);
        if (p->startDrag(event)) {
            setMouseGrabberPopup(drawer);
            return true;
        }
    }

    return false;
}

bool QQuickOverlayPrivate::handlePress(QQuickItem *source, QEvent *event, QQuickPopup *target)
{
    if (target) {
        if (target->overlayEvent(source, event)) {
            setMouseGrabberPopup(target);
            return true;
        }
        return false;
    } else if (!mouseGrabberPopup) {
        // allow non-modal popups to close themselves,
        // and non-dimming modal popups to block the event
        const auto popups = stackingOrderPopups();
        for (QQuickPopup *popup : popups) {
            if (popup->overlayEvent(source, event)) {
                setMouseGrabberPopup(popup);
                return true;
            }
        }
    }

    event->ignore();
    return false;
}

bool QQuickOverlayPrivate::handleMove(QQuickItem *source, QEvent *event, QQuickPopup *target)
{
    if (target)
        return target->overlayEvent(source, event);
    return false;
}

bool QQuickOverlayPrivate::handleRelease(QQuickItem *source, QEvent *event, QQuickPopup *target)
{
    if (target) {
        setMouseGrabberPopup(nullptr);
        if (target->overlayEvent(source, event)) {
            setMouseGrabberPopup(nullptr);
            return true;
        }
    } else {
        const auto popups = stackingOrderPopups();
        for (QQuickPopup *popup : popups) {
            if (popup->overlayEvent(source, event))
                return true;
        }
    }
    return false;
}

bool QQuickOverlayPrivate::handleMouseEvent(QQuickItem *source, QMouseEvent *event, QQuickPopup *target)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (!target && startDrag(event, event->windowPos()))
            return true;
        return handlePress(source, event, target);
    case QEvent::MouseMove:
        return handleMove(source, event, target ? target : mouseGrabberPopup.data());
    case QEvent::MouseButtonRelease:
        return handleRelease(source, event, target ? target : mouseGrabberPopup.data());
    default:
        break;
    }
    return false;
}

#if QT_CONFIG(quicktemplates2_multitouch)
bool QQuickOverlayPrivate::handleTouchEvent(QQuickItem *source, QTouchEvent *event, QQuickPopup *target)
{
    bool handled = false;
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        for (const QTouchEvent::TouchPoint &point : event->touchPoints()) {
            switch (point.state()) {
            case Qt::TouchPointPressed:
                if (!target && startDrag(event, point.scenePos()))
                    handled = true;
                else
                    handled |= handlePress(source, event, target);
                break;
            case Qt::TouchPointMoved:
                handled |= handleMove(source, event, target ? target : mouseGrabberPopup.data());
                break;
            case Qt::TouchPointReleased:
                handled |= handleRelease(source, event, target ? target : mouseGrabberPopup.data());
                break;
            default:
                break;
            }
        }
        break;

    default:
        break;
    }

    return handled;
}
#endif

void QQuickOverlayPrivate::addPopup(QQuickPopup *popup)
{
    Q_Q(QQuickOverlay);
    allPopups += popup;
    if (QQuickDrawer *drawer = qobject_cast<QQuickDrawer *>(popup)) {
        allDrawers += drawer;
        q->setVisible(!allDrawers.isEmpty() || !q->childItems().isEmpty());
    }
}

void QQuickOverlayPrivate::removePopup(QQuickPopup *popup)
{
    Q_Q(QQuickOverlay);
    allPopups.removeOne(popup);
    if (allDrawers.removeOne(static_cast<QQuickDrawer *>(popup)))
        q->setVisible(!allDrawers.isEmpty() || !q->childItems().isEmpty());
}

void QQuickOverlayPrivate::setMouseGrabberPopup(QQuickPopup *popup)
{
    if (popup && !popup->isVisible())
        popup = nullptr;
    mouseGrabberPopup = popup;
}

QQuickOverlay::QQuickOverlay(QQuickItem *parent)
    : QQuickItem(*(new QQuickOverlayPrivate), parent)
{
    Q_D(QQuickOverlay);
    setZ(1000001); // DefaultWindowDecoration+1
    setAcceptedMouseButtons(Qt::AllButtons);
    setFiltersChildMouseEvents(true);
    setVisible(false);

    if (parent) {
        setSize(QSizeF(parent->width(), parent->height()));
        QQuickItemPrivate::get(parent)->addItemChangeListener(d, QQuickItemPrivate::Geometry);
        if (QQuickWindow *window = parent->window())
            window->installEventFilter(this);
    }
}

QQuickOverlay::~QQuickOverlay()
{
    Q_D(QQuickOverlay);
    if (QQuickItem *parent = parentItem())
        QQuickItemPrivate::get(parent)->removeItemChangeListener(d, QQuickItemPrivate::Geometry);
}

QQmlComponent *QQuickOverlay::modal() const
{
    Q_D(const QQuickOverlay);
    return d->modal;
}

void QQuickOverlay::setModal(QQmlComponent *modal)
{
    Q_D(QQuickOverlay);
    if (d->modal == modal)
        return;

    delete d->modal;
    d->modal = modal;
    emit modalChanged();
}

QQmlComponent *QQuickOverlay::modeless() const
{
    Q_D(const QQuickOverlay);
    return d->modeless;
}

void QQuickOverlay::setModeless(QQmlComponent *modeless)
{
    Q_D(QQuickOverlay);
    if (d->modeless == modeless)
        return;

    delete d->modeless;
    d->modeless = modeless;
    emit modelessChanged();
}

QQuickOverlay *QQuickOverlay::overlay(QQuickWindow *window)
{
    if (!window)
        return nullptr;

    QQuickApplicationWindow *applicationWindow = qobject_cast<QQuickApplicationWindow *>(window);
    if (applicationWindow)
        return applicationWindow->overlay();

    const char *name = "_q_QQuickOverlay";
    QQuickOverlay *overlay = window->property(name).value<QQuickOverlay *>();
    if (!overlay) {
        QQuickItem *content = window->contentItem();
        // Do not re-create the overlay if the window is being destroyed
        // and thus, its content item no longer has a window associated.
        if (content->window()) {
            overlay = new QQuickOverlay(window->contentItem());
            window->setProperty(name, QVariant::fromValue(overlay));
        }
    }
    return overlay;
}

void QQuickOverlay::itemChange(ItemChange change, const ItemChangeData &data)
{
    Q_D(QQuickOverlay);
    QQuickItem::itemChange(change, data);

    QQuickPopup *popup = nullptr;
    if (change == ItemChildAddedChange || change == ItemChildRemovedChange) {
        popup = qobject_cast<QQuickPopup *>(data.item->parent());
        setVisible(!d->allDrawers.isEmpty() || !childItems().isEmpty());
    }
    if (!popup)
        return;

    if (change == ItemChildAddedChange) {
        if (popup->dim())
            d->createOverlay(popup);
        QObjectPrivate::connect(popup, &QQuickPopup::dimChanged, d, &QQuickOverlayPrivate::toggleOverlay);
        QObjectPrivate::connect(popup, &QQuickPopup::modalChanged, d, &QQuickOverlayPrivate::toggleOverlay);
        if (!qobject_cast<QQuickDrawer *>(popup)) {
            QObjectPrivate::connect(popup, &QQuickPopup::aboutToShow, d, &QQuickOverlayPrivate::popupAboutToShow);
            QObjectPrivate::connect(popup, &QQuickPopup::aboutToHide, d, &QQuickOverlayPrivate::popupAboutToHide);
        }
    } else if (change == ItemChildRemovedChange) {
        d->destroyOverlay(popup);
        QObjectPrivate::disconnect(popup, &QQuickPopup::dimChanged, d, &QQuickOverlayPrivate::toggleOverlay);
        QObjectPrivate::disconnect(popup, &QQuickPopup::modalChanged, d, &QQuickOverlayPrivate::toggleOverlay);
        if (!qobject_cast<QQuickDrawer *>(popup)) {
            QObjectPrivate::disconnect(popup, &QQuickPopup::aboutToShow, d, &QQuickOverlayPrivate::popupAboutToShow);
            QObjectPrivate::disconnect(popup, &QQuickPopup::aboutToHide, d, &QQuickOverlayPrivate::popupAboutToHide);
        }
    }
}

void QQuickOverlay::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickOverlay);
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    for (QQuickPopup *popup : qAsConst(d->allPopups))
        QQuickPopupPrivate::get(popup)->resizeOverlay();
}

void QQuickOverlay::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickOverlay);
    d->handleMouseEvent(this, event);
}

void QQuickOverlay::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickOverlay);
    d->handleMouseEvent(this, event);
}

void QQuickOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickOverlay);
    d->handleMouseEvent(this, event);
}

#if QT_CONFIG(quicktemplates2_multitouch)
void QQuickOverlay::touchEvent(QTouchEvent *event)
{
    Q_D(QQuickOverlay);
    d->handleTouchEvent(this, event);
}
#endif

#if QT_CONFIG(wheelevent)
void QQuickOverlay::wheelEvent(QWheelEvent *event)
{
    Q_D(QQuickOverlay);
    if (d->mouseGrabberPopup) {
        d->mouseGrabberPopup->overlayEvent(this, event);
        return;
    } else {
        const auto popups = d->stackingOrderPopups();
        for (QQuickPopup *popup : popups) {
            if (popup->overlayEvent(this, event))
                return;
        }
    }
    event->ignore();
}
#endif

bool QQuickOverlay::childMouseEventFilter(QQuickItem *item, QEvent *event)
{
    Q_D(QQuickOverlay);
    const auto popups = d->stackingOrderPopups();
    for (QQuickPopup *popup : popups) {
        QQuickPopupPrivate *p = QQuickPopupPrivate::get(popup);

        // Stop filtering overlay events when reaching a popup item or an item
        // that is inside the popup. Let the popup content handle its events.
        if (item == p->popupItem || p->popupItem->isAncestorOf(item))
            break;

        // Let the popup try closing itself when pressing or releasing over its
        // background dimming OR over another popup underneath, in case the popup
        // does not have background dimming.
        if (item == p->dimmer || !p->popupItem->isAncestorOf(item)) {
            switch (event->type()) {
#if QT_CONFIG(quicktemplates2_multitouch)
            case QEvent::TouchBegin:
            case QEvent::TouchUpdate:
            case QEvent::TouchEnd:
                return d->handleTouchEvent(item, static_cast<QTouchEvent *>(event), popup);
#endif

            case QEvent::MouseButtonPress:
            case QEvent::MouseMove:
            case QEvent::MouseButtonRelease:
                return d->handleMouseEvent(item, static_cast<QMouseEvent *>(event), popup);

            default:
                break;
            }
        }
    }
    return false;
}

bool QQuickOverlay::eventFilter(QObject *object, QEvent *event)
{
    Q_D(QQuickOverlay);
    if (!isVisible() || object != d->window)
        return false;

    switch (event->type()) {
#if QT_CONFIG(quicktemplates2_multitouch)
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        if (static_cast<QTouchEvent *>(event)->touchPointStates() & Qt::TouchPointPressed)
            emit pressed();
        if (static_cast<QTouchEvent *>(event)->touchPointStates() & Qt::TouchPointReleased)
            emit released();

        // allow non-modal popups to close on touch release outside
        if (!d->mouseGrabberPopup) {
            for (const QTouchEvent::TouchPoint &point : static_cast<QTouchEvent *>(event)->touchPoints()) {
                if (point.state() == Qt::TouchPointReleased) {
                    if (d->handleRelease(d->window->contentItem(), event, nullptr))
                        break;
                }
            }
        }

        QQuickWindowPrivate::get(d->window)->handleTouchEvent(static_cast<QTouchEvent *>(event));

        // If a touch event hasn't been accepted after being delivered, there
        // were no items interested in touch events at any of the touch points.
        // Make sure to accept the touch event in order to receive the consequent
        // touch events, to be able to close non-modal popups on release outside.
        event->accept();
        return true;
#endif

    case QEvent::MouseButtonPress:
#if QT_CONFIG(quicktemplates2_multitouch)
        // do not emit pressed() twice when mouse events have been synthesized from touch events
        if (static_cast<QMouseEvent *>(event)->source() == Qt::MouseEventNotSynthesized)
#endif
            emit pressed();

        QQuickWindowPrivate::get(d->window)->handleMouseEvent(static_cast<QMouseEvent *>(event));

        // If a mouse event hasn't been accepted after being delivered, there
        // was no item interested in mouse events at the mouse point. Make sure
        // to accept the mouse event in order to receive the consequent mouse
        // events, to be able to close non-modal popups on release outside.
        event->accept();
        return true;

    case QEvent::MouseButtonRelease:
#if QT_CONFIG(quicktemplates2_multitouch)
        // do not emit released() twice when mouse events have been synthesized from touch events
        if (static_cast<QMouseEvent *>(event)->source() == Qt::MouseEventNotSynthesized)
#endif
            emit released();

        // allow non-modal popups to close on mouse release outside
        if (!d->mouseGrabberPopup)
            d->handleRelease(d->window->contentItem(), event, nullptr);
        break;

    default:
        break;
    }

    return false;
}

QT_END_NAMESPACE
