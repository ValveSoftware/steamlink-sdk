/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#ifndef QWAYLANDWLSHELLINTEGRATION_H
#define QWAYLANDWLSHELLINTEGRATION_H

#include <QtWaylandCompositor/private/qwaylandquickshellsurfaceitem_p.h>

#include <QtWaylandCompositor/QWaylandWlShellSurface>

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

class WlShellIntegration : public QWaylandQuickShellIntegration
{
    Q_OBJECT
public:
    WlShellIntegration(QWaylandQuickShellSurfaceItem *item);
    ~WlShellIntegration();
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;

private Q_SLOTS:
    void handleStartMove(QWaylandSeat *seat);
    void handleStartResize(QWaylandSeat *seat, QWaylandWlShellSurface::ResizeEdge edges);
    void handleSetDefaultTopLevel();
    void handleSetTransient(QWaylandSurface *parentSurface, const QPoint &relativeToParent, bool inactive);
    void handleSetMaximized(QWaylandOutput *output);
    void handleSetFullScreen(QWaylandWlShellSurface::FullScreenMethod method, uint framerate, QWaylandOutput *output);
    void handleSetPopup(QWaylandSeat *seat, QWaylandSurface *parent, const QPoint &relativeToParent);
    void handleShellSurfaceDestroyed();
    void handleSurfaceHasContentChanged();
    void handleRedraw();
    void adjustOffsetForNextFrame(const QPointF &offset);

private:
    enum class GrabberState {
        Default,
        Resize,
        Move
    };

    void handlePopupClosed();
    void handlePopupRemoved();
    qreal devicePixelRatio() const;

    QWaylandQuickShellSurfaceItem *m_item;
    QPointer<QWaylandWlShellSurface> m_shellSurface;
    GrabberState grabberState;
    struct {
        QWaylandSeat *seat;
        QPointF initialOffset;
        bool initialized;
    } moveState;
    struct {
        QWaylandSeat *seat;
        QWaylandWlShellSurface::ResizeEdge resizeEdges;
        QSizeF initialSize;
        QPointF initialMousePos;
        bool initialized;
    } resizeState;

    bool isPopup;

    enum class State {
        Windowed,
        Maximized,
        FullScreen
    };

    State currentState;
    State nextState;
    QPointF normalPosition;
    QPointF finalPosition;
};

}

QT_END_NAMESPACE

#endif // QWAYLANDWLSHELLINTEGRATION_H
