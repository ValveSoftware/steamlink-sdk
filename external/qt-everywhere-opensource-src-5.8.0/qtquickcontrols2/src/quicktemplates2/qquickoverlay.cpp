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

#include "qquickoverlay_p.h"
#include "qquickoverlay_p_p.h"
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
        QQmlContext *context = new QQmlContext(creationContext);
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
            // TODO: switch to QStyleHints::useHoverEffects in Qt 5.8
            item->setAcceptHoverEvents(true);
            // item->setAcceptHoverEvents(QGuiApplication::styleHints()->useHoverEffects());
            // connect(QGuiApplication::styleHints(), &QStyleHints::useHoverEffectsChanged, item, &QQuickItem::setAcceptHoverEvents);
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

QQuickOverlayPrivate::QQuickOverlayPrivate() :
    modal(nullptr),
    modeless(nullptr)
{
}

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
    emit pressed();

    if (!d->allDrawers.isEmpty()) {
        // the overlay background was pressed, so there are no modal popups open.
        // test if the press point lands on any drawer's drag margin

        const QVector<QQuickDrawer *> drawers = d->stackingOrderDrawers();
        for (QQuickDrawer *drawer : drawers) {
            QQuickDrawerPrivate *p = QQuickDrawerPrivate::get(drawer);
            if (p->startDrag(window(), event)) {
                d->setMouseGrabberPopup(drawer);
                return;
            }
        }
    }

    if (!d->mouseGrabberPopup) {
        const auto popups = d->stackingOrderPopups();
        for (QQuickPopup *popup : popups) {
            if (popup->overlayEvent(this, event)) {
                d->setMouseGrabberPopup(popup);
                return;
            }
        }
    }

    event->ignore();
}

void QQuickOverlay::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickOverlay);
    if (d->mouseGrabberPopup)
        d->mouseGrabberPopup->overlayEvent(this, event);
}

void QQuickOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickOverlay);
    emit released();

    if (d->mouseGrabberPopup) {
        d->mouseGrabberPopup->overlayEvent(this, event);
        d->setMouseGrabberPopup(nullptr);
    } else {
        const auto popups = d->stackingOrderPopups();
        for (QQuickPopup *popup : popups) {
            if (popup->overlayEvent(this, event))
                break;
        }
    }
}

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
            case QEvent::MouseButtonPress:
                emit pressed();
                if (popup->overlayEvent(item, event)) {
                    d->setMouseGrabberPopup(popup);
                    return true;
                }
                break;
            case QEvent::MouseMove:
                return popup->overlayEvent(item, event);
            case QEvent::MouseButtonRelease:
                emit released();
                d->setMouseGrabberPopup(nullptr);
                return popup->overlayEvent(item, event);
            default:
                break;
            }
        }
    }
    return false;
}

QT_END_NAMESPACE
