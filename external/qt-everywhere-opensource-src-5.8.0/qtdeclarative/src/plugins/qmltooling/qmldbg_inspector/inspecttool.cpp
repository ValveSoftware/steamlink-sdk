/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "inspecttool.h"
#include "highlight.h"
#include "qquickwindowinspector.h"
#include "globalinspector.h"

#include <QtCore/QLineF>

#include <QtGui/QMouseEvent>
#include <QtGui/QTouchEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>

#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>

QT_BEGIN_NAMESPACE

namespace QmlJSDebugger {

InspectTool::InspectTool(QQuickWindowInspector *inspector, QQuickWindow *view) :
    QObject(inspector),
    m_contentItem(view->contentItem()),
    m_touchTimestamp(0),
    m_hoverHighlight(new HoverHighlight(inspector->overlay())),
    m_lastItem(0),
    m_lastClickedItem(0)
{
    //Timer to display selected item's name
    m_nameDisplayTimer.setSingleShot(true);
    m_nameDisplayTimer.setInterval(QGuiApplication::styleHints()->mouseDoubleClickInterval());
    connect(&m_nameDisplayTimer, &QTimer::timeout, this, &InspectTool::showItemName);
}

void InspectTool::enterEvent(QEvent *)
{
    m_hoverHighlight->setVisible(true);
}

void InspectTool::leaveEvent(QEvent *)
{
    m_hoverHighlight->setVisible(false);
}

void InspectTool::mousePressEvent(QMouseEvent *event)
{
    m_mousePosition = event->localPos();
    if (event->button() == Qt::LeftButton) {
        selectItem();
        m_hoverHighlight->setVisible(false);
    }
}

void InspectTool::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_mousePosition = event->localPos();
    if (event->button() == Qt::LeftButton) {
        selectNextItem();
        m_hoverHighlight->setVisible(false);
    }
}

void InspectTool::mouseMoveEvent(QMouseEvent *event)
{
    hoverMoveEvent(event);
}

void InspectTool::hoverMoveEvent(QMouseEvent *event)
{
    m_mousePosition = event->localPos();
    QQuickItem *item = inspector()->topVisibleItemAt(event->pos());
    if (!item || item == m_lastClickedItem) {
        m_hoverHighlight->setVisible(false);
    } else {
        m_hoverHighlight->setItem(item);
        m_hoverHighlight->setVisible(true);
    }
}

void InspectTool::touchEvent(QTouchEvent *event)
{
    QList<QTouchEvent::TouchPoint> touchPoints = event->touchPoints();

    switch (event->type()) {
    case QEvent::TouchBegin:
        if (touchPoints.count() == 1 && (event->touchPointStates() & Qt::TouchPointPressed)) {
            m_mousePosition = touchPoints.first().pos();
            m_tapEvent = true;
        } else {
            m_tapEvent = false;
        }
        break;
    case QEvent::TouchUpdate: {
        if (touchPoints.count() > 1)
            m_tapEvent = false;
        else if ((touchPoints.count() == 1) && (event->touchPointStates() & Qt::TouchPointMoved))
            m_mousePosition = touchPoints.first().pos();
        break;
    }
    case QEvent::TouchEnd: {
        if (touchPoints.count() == 1 && m_tapEvent) {
            m_tapEvent = false;
            bool doubleTap = event->timestamp() - m_touchTimestamp
                    < static_cast<ulong>(QGuiApplication::styleHints()->mouseDoubleClickInterval());
            if (doubleTap) {
                m_nameDisplayTimer.stop();
                selectNextItem();
            } else {
                selectItem();
            }
            m_touchTimestamp = event->timestamp();
        }
        break;
    }
    default:
        break;
    }
}

void InspectTool::selectNextItem()
{
    if (m_lastClickedItem != inspector()->topVisibleItemAt(m_mousePosition))
        return;
    QList<QQuickItem*> items = inspector()->itemsAt(m_mousePosition);
    for (int i = 0; i < items.count(); i++) {
        if (m_lastItem == items[i]) {
            if (i + 1 < items.count())
                m_lastItem = items[i+1];
            else
                m_lastItem = items[0];
            globalInspector()->setSelectedItems(QList<QQuickItem*>() << m_lastItem);
            showItemName();
            break;
        }
    }
}

void InspectTool::selectItem()
{
    if (!inspector()->topVisibleItemAt(m_mousePosition))
        return;
    m_lastClickedItem = inspector()->topVisibleItemAt(m_mousePosition);
    m_lastItem = m_lastClickedItem;
    globalInspector()->setSelectedItems(QList<QQuickItem*>() << m_lastClickedItem);
    if (m_lastClickedItem == inspector()->topVisibleItemAt(m_mousePosition)) {
        m_nameDisplayTimer.start();
    } else {
        showItemName();
    }
}

void InspectTool::showItemName()
{
    globalInspector()->showSelectedItemName(m_lastItem, m_mousePosition);
}

QQuickWindowInspector *InspectTool::inspector() const
{
    return static_cast<QQuickWindowInspector *>(parent());
}

GlobalInspector *InspectTool::globalInspector() const
{
    return static_cast<GlobalInspector *>(parent()->parent());
}

} // namespace QmlJSDebugger

QT_END_NAMESPACE
