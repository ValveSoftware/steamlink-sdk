/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QWAYLANDXDGSHELLV5INTEGRATION_H
#define QWAYLANDXDGSHELLV5INTEGRATION_H

#include <QtWaylandCompositor/private/qwaylandquickshellsurfaceitem_p.h>
#include <QtWaylandCompositor/QWaylandXdgSurfaceV5>

QT_BEGIN_NAMESPACE

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

namespace QtWayland {

class XdgShellV5Integration : public QWaylandQuickShellIntegration
{
    Q_OBJECT
public:
    XdgShellV5Integration(QWaylandQuickShellSurfaceItem *item);
    bool mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    bool mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void handleStartMove(QWaylandSeat *seat);
    void handleStartResize(QWaylandSeat *seat, QWaylandXdgSurfaceV5::ResizeEdge edges);
    void handleSetTopLevel();
    void handleSetTransient();
    void handleSetMaximized();
    void handleUnsetMaximized();
    void handleMaximizedChanged();
    void handleActivatedChanged();
    void handleSurfaceSizeChanged();

private:
    enum class GrabberState {
        Default,
        Resize,
        Move
    };
    QWaylandQuickShellSurfaceItem *m_item;
    QWaylandXdgSurfaceV5 *m_xdgSurface;

    GrabberState grabberState;
    struct {
        QWaylandSeat *seat;
        QPointF initialOffset;
        bool initialized;
    } moveState;

    struct {
        QWaylandSeat *seat;
        QWaylandXdgSurfaceV5::ResizeEdge resizeEdges;
        QSizeF initialWindowSize;
        QPointF initialMousePos;
        QPointF initialPosition;
        QSize initialSurfaceSize;
        bool initialized;
    } resizeState;

    struct {
        QSize initialWindowSize;
        QPointF initialPosition;
    } maximizeState;
};

class XdgPopupV5Integration : public QWaylandQuickShellIntegration
{
    Q_OBJECT
public:
    XdgPopupV5Integration(QWaylandQuickShellSurfaceItem *item);

private Q_SLOTS:
    void handlePopupDestroyed();

private:
    QWaylandXdgPopupV5 *m_xdgPopup;
    QWaylandXdgShellV5 *m_xdgShell;
};

}

QT_END_NAMESPACE

#endif // QWAYLANDXDGSHELLINTEGRATION_H
