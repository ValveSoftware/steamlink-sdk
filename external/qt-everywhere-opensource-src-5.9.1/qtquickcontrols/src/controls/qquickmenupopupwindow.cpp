/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickmenupopupwindow_p.h"

#include <qguiapplication.h>
#include <qpa/qwindowsysteminterface.h>
#include <qquickitem.h>
#include <QtGui/QScreen>
#include <QtQuick/QQuickRenderControl>
#include "qquickmenu_p.h"
#include "qquickmenubar_p.h"

QT_BEGIN_NAMESPACE

QQuickMenuPopupWindow1::QQuickMenuPopupWindow1(QQuickMenu1 *menu) :
    m_itemAt(0),
    m_logicalParentWindow(0),
    m_menu(menu)
{
}

void QQuickMenuPopupWindow1::setParentItem(QQuickItem *item)
{
    QQuickPopupWindow1::setParentItem(item);
    if (item) {
        QWindow *parentWindow = item->window();
        QWindow *renderWindow = QQuickRenderControl::renderWindowFor(static_cast<QQuickWindow *>(parentWindow));
        setParentWindow(renderWindow ? renderWindow : parentWindow, item->window());
    }
}

void QQuickMenuPopupWindow1::setItemAt(QQuickItem *menuItem)
{
    if (m_itemAt) {
        disconnect(m_itemAt, SIGNAL(xChanged()), this, SLOT(updatePosition()));
        disconnect(m_itemAt, SIGNAL(yChanged()), this, SLOT(updatePosition()));
    }

    m_itemAt = menuItem;
    if (menuItem) {
        m_oldItemPos = menuItem->position().toPoint();
        connect(menuItem, SIGNAL(xChanged()), this, SLOT(updatePosition()));
        connect(menuItem, SIGNAL(yChanged()), this, SLOT(updatePosition()));
    }
}

void QQuickMenuPopupWindow1::setParentWindow(QWindow *effectiveParentWindow, QQuickWindow *parentWindow)
{
    while (effectiveParentWindow && effectiveParentWindow->parent())
        effectiveParentWindow = effectiveParentWindow->parent();
    if (transientParent() != effectiveParentWindow)
        setTransientParent(effectiveParentWindow);
    m_logicalParentWindow = parentWindow;
    if (parentWindow) {
        if (QQuickMenuPopupWindow1 *pw = qobject_cast<QQuickMenuPopupWindow1 *>(parentWindow)) {
            connect(pw, SIGNAL(popupDismissed()), this, SLOT(dismissPopup()));
            connect(pw, SIGNAL(willBeDeletedLater()), this, SLOT(setToBeDeletedLater()));
        } else {
            connect(parentWindow, SIGNAL(destroyed()), this, SLOT(deleteLater()));
        }
    }
}

void QQuickMenuPopupWindow1::setToBeDeletedLater()
{
    deleteLater();
    emit willBeDeletedLater();
}

void QQuickMenuPopupWindow1::setGeometry(int posx, int posy, int w, int h)
{
    QWindow *pw = transientParent();
    if (!pw && parentItem())
        pw = parentItem()->window();
    if (!pw)
        pw = this;
    QRect g = pw->screen()->geometry();

    if (posx + w > g.right()) {
        if (qobject_cast<QQuickMenuPopupWindow1 *>(transientParent())) {
            // reposition submenu window on the parent menu's left side
            int submenuOverlap = pw->x() + pw->width() - posx;
            posx -= pw->width() + w - 2 * submenuOverlap;
        } else {
            posx = g.right() - w;
        }
    } else {
        posx = qMax(posx, g.left());
    }

    posy = qBound(g.top(), posy, g.bottom() - h);

    QQuickWindow::setGeometry(posx, posy, w, h);
    emit geometryChanged();
}

void QQuickMenuPopupWindow1::updateSize()
{
    const QQuickItem *contentItem = popupContentItem();
    if (!contentItem)
        return;

    qreal x = m_initialPos.x();
    qreal y = m_initialPos.y();
    if (qGuiApp->layoutDirection() == Qt::RightToLeft)
        x -= contentItem->width();
    setGeometry(x, y, contentItem->width(), contentItem->height());
}

void QQuickMenuPopupWindow1::updatePosition()
{
    QPointF newPos = position() + m_oldItemPos - m_itemAt->position();
    m_initialPos += m_oldItemPos - m_itemAt->position();
    setGeometry(newPos.x(), newPos.y(), width(), height());
}

void QQuickMenuPopupWindow1::focusInEvent(QFocusEvent *e)
{
    QQuickWindow::focusInEvent(e);
    if (m_menu && m_menu->menuContentItem())
        m_menu->menuContentItem()->forceActiveFocus();
}

void QQuickMenuPopupWindow1::exposeEvent(QExposeEvent *e)
{
    // the popup will reposition at the last moment, so its
    // initial position must be captured for updateSize().
    m_initialPos = position();
    if (m_logicalParentWindow && m_logicalParentWindow->parent()) {
        // This must be a QQuickWindow embedded via createWindowContainer.
        m_initialPos += m_logicalParentWindow->geometry().topLeft();
    }
    QQuickPopupWindow1::exposeEvent(e);

    if (isExposed())
        updateSize();
}

QQuickMenu1 *QQuickMenuPopupWindow1::menu() const
{
    return m_menu;
}

bool QQuickMenuPopupWindow1::shouldForwardEventAfterDismiss(QMouseEvent *e) const
{
    // If the event falls inside this item the event should not be forwarded.
    // For example for comboboxes or top menus of the menubar
    QQuickMenuBar1 *mb = m_menu ? m_menu->menuBar() : Q_NULLPTR;
    QQuickItem *item = mb && !mb->isNative() ? mb->contentItem() : menu()->visualItem();
    QWindow *window = transientParent();
    if (item && window && item->window() == window) {
        QPointF pos = window->mapFromGlobal(mapToGlobal(e->pos()));
        pos = item->mapFromScene(pos);
        if (item->contains(pos))
            return false;
    }

#ifdef Q_OS_OSX
    if (e->button() == Qt::RightButton)
        return true;
#endif

#ifdef Q_OS_WIN
    return true;
#endif

    return false;
}

QT_END_NAMESPACE
