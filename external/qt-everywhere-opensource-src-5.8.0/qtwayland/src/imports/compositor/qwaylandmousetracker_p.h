/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#ifndef QWAYLANDMOUSETRACKER_P_H
#define QWAYLANDMOUSETRACKER_P_H

#include <QtQuick/private/qquickmousearea_p.h>

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>

QT_BEGIN_NAMESPACE

class QWaylandMouseTrackerPrivate;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandMouseTracker : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandMouseTracker)
    Q_PROPERTY(qreal mouseX READ mouseX NOTIFY mouseXChanged)
    Q_PROPERTY(qreal mouseY READ mouseY NOTIFY mouseYChanged)
    Q_PROPERTY(bool containsMouse READ hovered NOTIFY hoveredChanged)

    Q_PROPERTY(bool windowSystemCursorEnabled READ windowSystemCursorEnabled WRITE setWindowSystemCursorEnabled NOTIFY windowSystemCursorEnabledChanged)
public:
    QWaylandMouseTracker(QQuickItem *parent = 0);

    qreal mouseX() const;
    qreal mouseY() const;

    void setWindowSystemCursorEnabled(bool enable);
    bool windowSystemCursorEnabled() const;
    bool hovered() const;

signals:
    void mouseXChanged();
    void mouseYChanged();
    void windowSystemCursorEnabledChanged();
    void hoveredChanged();

protected:
    bool childMouseEventFilter(QQuickItem *item, QEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void hoverMoveEvent(QHoverEvent *event) Q_DECL_OVERRIDE;
    void hoverEnterEvent(QHoverEvent *event) Q_DECL_OVERRIDE;
    void hoverLeaveEvent(QHoverEvent *event) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif  /*QWAYLANDMOUSETRACKER_P_H*/
