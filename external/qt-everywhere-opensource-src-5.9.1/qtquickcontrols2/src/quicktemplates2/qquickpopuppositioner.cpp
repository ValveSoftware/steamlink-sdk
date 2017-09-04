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

#include "qquickpopuppositioner_p_p.h"
#include "qquickpopupitem_p_p.h"
#include "qquickpopup_p_p.h"

#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

static const QQuickItemPrivate::ChangeTypes AncestorChangeTypes = QQuickItemPrivate::Geometry
                                                                  | QQuickItemPrivate::Parent
                                                                  | QQuickItemPrivate::Children;

static const QQuickItemPrivate::ChangeTypes ItemChangeTypes = QQuickItemPrivate::Geometry
                                                             | QQuickItemPrivate::Parent;

QQuickPopupPositioner::QQuickPopupPositioner(QQuickPopup *popup)
    : m_positioning(false),
      m_parentItem(nullptr),
      m_popup(popup)
{
}

QQuickPopupPositioner::~QQuickPopupPositioner()
{
    if (m_parentItem) {
        QQuickItemPrivate::get(m_parentItem)->removeItemChangeListener(this, ItemChangeTypes);
        removeAncestorListeners(m_parentItem->parentItem());
    }
}

QQuickItem *QQuickPopupPositioner::parentItem() const
{
    return m_parentItem;
}

void QQuickPopupPositioner::setParentItem(QQuickItem *parent)
{
    if (m_parentItem == parent)
        return;

    if (m_parentItem) {
        QQuickItemPrivate::get(m_parentItem)->removeItemChangeListener(this, ItemChangeTypes);
        removeAncestorListeners(m_parentItem->parentItem());
    }

    m_parentItem = parent;

    if (!parent)
        return;

    QQuickItemPrivate::get(parent)->addItemChangeListener(this, ItemChangeTypes);
    addAncestorListeners(parent->parentItem());

    if (m_popup->popupItem()->isVisible())
        QQuickPopupPrivate::get(m_popup)->reposition();
}

void QQuickPopupPositioner::reposition()
{
    QQuickItem *popupItem = m_popup->popupItem();
    if (!popupItem->isVisible())
        return;

    if (m_positioning) {
        popupItem->polish();
        return;
    }

    const qreal w = popupItem->width();
    const qreal h = popupItem->height();
    const qreal iw = popupItem->implicitWidth();
    const qreal ih = popupItem->implicitHeight();

    bool widthAdjusted = false;
    bool heightAdjusted = false;
    QQuickPopupPrivate *p = QQuickPopupPrivate::get(m_popup);

    QRectF rect(p->allowHorizontalMove ? p->x : popupItem->x(),
                p->allowVerticalMove ? p->y : popupItem->y(),
                !p->hasWidth && iw > 0 ? iw : w,
                !p->hasHeight && ih > 0 ? ih : h);
    if (m_parentItem) {
        rect = m_parentItem->mapRectToScene(rect);

        if (p->window) {
            const QMarginsF margins = p->getMargins();
            const QRectF bounds(qMax<qreal>(0.0, margins.left()),
                                qMax<qreal>(0.0, margins.top()),
                                p->window->width() - qMax<qreal>(0.0, margins.left()) - qMax<qreal>(0.0, margins.right()),
                                p->window->height() - qMax<qreal>(0.0, margins.top()) - qMax<qreal>(0.0, margins.bottom()));

            // if the popup doesn't fit horizontally inside the window, try flipping it around (left <-> right)
            if (p->allowHorizontalFlip && (rect.left() < bounds.left() || rect.right() > bounds.right())) {
                const QRectF flipped = m_parentItem->mapRectToScene(QRectF(m_parentItem->width() - p->x - rect.width(), p->y, rect.width(), rect.height()));
                if (flipped.intersected(bounds).width() > rect.intersected(bounds).width())
                    rect.moveLeft(flipped.left());
            }

            // if the popup doesn't fit vertically inside the window, try flipping it around (above <-> below)
            if (p->allowVerticalFlip && (rect.top() < bounds.top() || rect.bottom() > bounds.bottom())) {
                const QRectF flipped = m_parentItem->mapRectToScene(QRectF(p->x, m_parentItem->height() - p->y - rect.height(), rect.width(), rect.height()));
                if (flipped.intersected(bounds).height() > rect.intersected(bounds).height())
                    rect.moveTop(flipped.top());
            }

            // push inside the margins if specified
            if (p->allowVerticalMove) {
                if (margins.top() >= 0 && rect.top() < bounds.top())
                    rect.moveTop(margins.top());
                if (margins.bottom() >= 0 && rect.bottom() > bounds.bottom())
                    rect.moveBottom(bounds.bottom());
            }
            if (p->allowHorizontalMove) {
                if (margins.left() >= 0 && rect.left() < bounds.left())
                    rect.moveLeft(margins.left());
                if (margins.right() >= 0 && rect.right() > bounds.right())
                    rect.moveRight(bounds.right());
            }

            if (iw > 0 && (rect.left() < bounds.left() || rect.right() > bounds.right())) {
                // neither the flipped or pushed geometry fits inside the window, choose
                // whichever side (left vs. right) fits larger part of the popup
                if (p->allowHorizontalMove && p->allowHorizontalFlip) {
                    if (rect.left() < bounds.left() && bounds.left() + rect.width() <= bounds.right())
                        rect.moveLeft(bounds.left());
                    else if (rect.right() > bounds.right() && bounds.right() - rect.width() >= bounds.left())
                        rect.moveRight(bounds.right());
                }

                // as a last resort, adjust the width to fit the window
                if (p->allowHorizontalResize) {
                    if (rect.left() < bounds.left()) {
                        rect.setLeft(bounds.left());
                        widthAdjusted = true;
                    }
                    if (rect.right() > bounds.right()) {
                        rect.setRight(bounds.right());
                        widthAdjusted = true;
                    }
                }
            } else if (iw > 0 && rect.left() >= bounds.left() && rect.right() <= bounds.right()
                       && iw != w) {
                // restore original width
                rect.setWidth(iw);
                widthAdjusted = true;
            }

            if (ih > 0 && (rect.top() < bounds.top() || rect.bottom() > bounds.bottom())) {
                // neither the flipped or pushed geometry fits inside the window, choose
                // whichever side (above vs. below) fits larger part of the popup
                if (p->allowVerticalMove && p->allowVerticalFlip) {
                    if (rect.top() < bounds.top() && bounds.top() + rect.height() <= bounds.bottom())
                        rect.moveTop(bounds.top());
                    else if (rect.bottom() > bounds.bottom() && bounds.bottom() - rect.height() >= bounds.top())
                        rect.moveBottom(bounds.bottom());
                }

                // as a last resort, adjust the height to fit the window
                if (p->allowVerticalResize) {
                    if (rect.top() < bounds.top()) {
                        rect.setTop(bounds.top());
                        heightAdjusted = true;
                    }
                    if (rect.bottom() > bounds.bottom()) {
                        rect.setBottom(bounds.bottom());
                        heightAdjusted = true;
                    }
                }
            } else if (ih > 0 && rect.top() >= bounds.top() && rect.bottom() <= bounds.bottom()
                       && ih != h) {
                // restore original height
                rect.setHeight(ih);
                heightAdjusted = true;
            }
        }
    }

    m_positioning = true;

    popupItem->setPosition(rect.topLeft());

    const QPointF effectivePos = m_parentItem ? m_parentItem->mapFromScene(rect.topLeft()) : rect.topLeft();
    if (!qFuzzyCompare(p->effectiveX, effectivePos.x())) {
        p->effectiveX = effectivePos.x();
        emit m_popup->xChanged();
    }
    if (!qFuzzyCompare(p->effectiveY, effectivePos.y())) {
        p->effectiveY = effectivePos.y();
        emit m_popup->yChanged();
    }

    if (!p->hasWidth && widthAdjusted && rect.width() > 0)
        popupItem->setWidth(rect.width());
    if (!p->hasHeight && heightAdjusted && rect.height() > 0)
        popupItem->setHeight(rect.height());

    m_positioning = false;
}

void QQuickPopupPositioner::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    if (m_parentItem && m_popup->popupItem()->isVisible())
        QQuickPopupPrivate::get(m_popup)->reposition();
}

void QQuickPopupPositioner::itemParentChanged(QQuickItem *, QQuickItem *parent)
{
    addAncestorListeners(parent);
}

void QQuickPopupPositioner::itemChildRemoved(QQuickItem *item, QQuickItem *child)
{
    if (child->isAncestorOf(m_parentItem))
        removeAncestorListeners(item);
}

void QQuickPopupPositioner::removeAncestorListeners(QQuickItem *item)
{
    if (item == m_parentItem)
        return;

    QQuickItem *p = item;
    while (p) {
        QQuickItemPrivate::get(p)->removeItemChangeListener(this, AncestorChangeTypes);
        p = p->parentItem();
    }
}

void QQuickPopupPositioner::addAncestorListeners(QQuickItem *item)
{
    if (item == m_parentItem)
        return;

    QQuickItem *p = item;
    while (p) {
        QQuickItemPrivate::get(p)->addItemChangeListener(this, AncestorChangeTypes);
        p = p->parentItem();
    }
}

QT_END_NAMESPACE
