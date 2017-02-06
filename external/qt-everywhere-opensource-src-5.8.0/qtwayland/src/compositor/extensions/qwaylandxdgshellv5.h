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

#ifndef QWAYLANDXDGSHELLV5_H
#define QWAYLANDXDGSHELLV5_H

#include <QtWaylandCompositor/QWaylandCompositorExtension>
#include <QtWaylandCompositor/QWaylandResource>
#include <QtWaylandCompositor/QWaylandShell>
#include <QtWaylandCompositor/QWaylandShellSurface>

#include <QtCore/QRect>

struct wl_resource;

QT_BEGIN_NAMESPACE

class QWaylandXdgShellV5Private;
class QWaylandXdgSurfaceV5;
class QWaylandXdgSurfaceV5Private;
class QWaylandXdgPopupV5;
class QWaylandXdgPopupV5Private;

class QWaylandSurface;
class QWaylandSurfaceRole;
class QWaylandSeat;
class QWaylandOutput;
class QWaylandClient;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandXdgShellV5 : public QWaylandShellTemplate<QWaylandXdgShellV5>
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandXdgShellV5)
public:
    QWaylandXdgShellV5();
    QWaylandXdgShellV5(QWaylandCompositor *compositor);

    void initialize() Q_DECL_OVERRIDE;
    QWaylandClient *popupClient() const;

    static const struct wl_interface *interface();
    static QByteArray interfaceName();

public Q_SLOTS:
    uint ping(QWaylandClient *client);
    void closeAllPopups();

Q_SIGNALS:
    void xdgSurfaceRequested(QWaylandSurface *surface, const QWaylandResource &resource);
    void xdgSurfaceCreated(QWaylandXdgSurfaceV5 *xdgSurface);
    void xdgPopupCreated(QWaylandXdgPopupV5 *xdgPopup);
    void xdgPopupRequested(QWaylandSurface *surface, QWaylandSurface *parent, QWaylandSeat *seat, const QPoint &position, const QWaylandResource &resource);
    void pong(uint serial);

private Q_SLOTS:
    void handleSeatChanged(QWaylandSeat *newSeat, QWaylandSeat *oldSeat);
    void handleFocusChanged(QWaylandSurface *newSurface, QWaylandSurface *oldSurface);

};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandXdgSurfaceV5 : public QWaylandShellSurfaceTemplate<QWaylandXdgSurfaceV5>
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandXdgSurfaceV5)
    Q_PROPERTY(QWaylandXdgShellV5 *shell READ shell NOTIFY shellChanged)
    Q_PROPERTY(QWaylandSurface *surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(QWaylandXdgSurfaceV5 *parentSurface READ parentSurface NOTIFY parentSurfaceChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString appId READ appId NOTIFY appIdChanged)
    Q_PROPERTY(QRect windowGeometry READ windowGeometry NOTIFY windowGeometryChanged)

    Q_PROPERTY(QList<int> states READ statesAsInts NOTIFY statesChanged)
    Q_PROPERTY(bool maximized READ maximized NOTIFY maximizedChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool resizing READ resizing NOTIFY resizingChanged)
    Q_PROPERTY(bool activated READ activated NOTIFY activatedChanged)

public:
    enum State : uint {
        MaximizedState  = 1,
        FullscreenState = 2,
        ResizingState   = 3,
        ActivatedState  = 4
    };
    Q_ENUM(State)

    enum ResizeEdge : uint {
        NoneEdge        =  0,
        TopEdge         =  1,
        BottomEdge      =  2,
        LeftEdge        =  4,
        TopLeftEdge     =  5,
        BottomLeftEdge  =  6,
        RightEdge       =  8,
        TopRightEdge    =  9,
        BottomRightEdge = 10
    };
    Q_ENUM(ResizeEdge)

    QWaylandXdgSurfaceV5();
    QWaylandXdgSurfaceV5(QWaylandXdgShellV5* xdgShell, QWaylandSurface *surface, const QWaylandResource &resource);

    Q_INVOKABLE void initialize(QWaylandXdgShellV5* xdgShell, QWaylandSurface *surface, const QWaylandResource &resource);

    QString title() const;
    QString appId() const;
    QRect windowGeometry() const;
    QVector<uint> states() const;
    bool maximized() const;
    bool fullscreen() const;
    bool resizing() const;
    bool activated() const;

    QWaylandXdgShellV5 *shell() const;

    QWaylandSurface *surface() const;
    QWaylandXdgSurfaceV5 *parentSurface() const;

    static const struct wl_interface *interface();
    static QByteArray interfaceName();
    static QWaylandSurfaceRole *role();
    static QWaylandXdgSurfaceV5 *fromResource(::wl_resource *resource);

    Q_INVOKABLE QSize sizeForResize(const QSizeF &size, const QPointF &delta, ResizeEdge edge);
    Q_INVOKABLE uint sendConfigure(const QSize &size, const QVector<uint> &states);
    Q_INVOKABLE uint sendConfigure(const QSize &size, const QVector<State> &states);
    Q_INVOKABLE void sendClose();

    Q_INVOKABLE uint sendMaximized(const QSize &size);
    Q_INVOKABLE uint sendUnmaximized(const QSize &size = QSize(0, 0));
    Q_INVOKABLE uint sendFullscreen(const QSize &size);
    Q_INVOKABLE uint sendResizing(const QSize &maxSize);

#ifdef QT_WAYLAND_COMPOSITOR_QUICK
    QWaylandQuickShellIntegration *createIntegration(QWaylandQuickShellSurfaceItem *item) Q_DECL_OVERRIDE;
#endif

Q_SIGNALS:
    void shellChanged();
    void surfaceChanged();
    void titleChanged();
    void windowGeometryChanged();
    void appIdChanged();
    void parentSurfaceChanged();

    void statesChanged();
    void maximizedChanged();
    void fullscreenChanged();
    void resizingChanged();
    void activatedChanged();

    void showWindowMenu(QWaylandSeat *seat, const QPoint &localSurfacePosition);
    void startMove(QWaylandSeat *seat);
    void startResize(QWaylandSeat *seat, ResizeEdge edges);
    void setTopLevel();
    void setTransient();
    void setMaximized();
    void unsetMaximized();
    void setFullscreen(QWaylandOutput *output);
    void unsetFullscreen();
    void setMinimized();
    void ackConfigure(uint serial);

private:
    void initialize() override;
    QList<int> statesAsInts() const;

private Q_SLOTS:
    void handleSurfaceSizeChanged();
    void handleBufferScaleChanged();
};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandXdgPopupV5 : public QWaylandShellSurfaceTemplate<QWaylandXdgPopupV5>
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandXdgPopupV5)
    Q_PROPERTY(QWaylandXdgShellV5 *shell READ shell NOTIFY shellChanged)
    Q_PROPERTY(QWaylandSurface *surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(QWaylandSurface *parentSurface READ parentSurface NOTIFY parentSurfaceChanged)
    Q_PROPERTY(QPoint position READ position NOTIFY positionChanged)

public:
    QWaylandXdgPopupV5();
    QWaylandXdgPopupV5(QWaylandXdgShellV5 *xdgShell, QWaylandSurface *surface, QWaylandSurface *parentSurface,
                     const QPoint &position, const QWaylandResource &resource);

    Qt::WindowType windowType() const override { return Qt::WindowType::Popup; }

    Q_INVOKABLE void initialize(QWaylandXdgShellV5 *shell, QWaylandSurface *surface,
                                QWaylandSurface *parentSurface, const QPoint &position, const QWaylandResource &resource);

    QWaylandXdgShellV5 *shell() const;

    QWaylandSurface *surface() const;
    QWaylandSurface *parentSurface() const;
    QPoint position() const;

    static const struct wl_interface *interface();
    static QByteArray interfaceName();
    static QWaylandSurfaceRole *role();
    static QWaylandXdgPopupV5 *fromResource(::wl_resource *resource);

    Q_INVOKABLE void sendPopupDone();

#ifdef QT_WAYLAND_COMPOSITOR_QUICK
    QWaylandQuickShellIntegration *createIntegration(QWaylandQuickShellSurfaceItem *item) Q_DECL_OVERRIDE;
#endif

Q_SIGNALS:
    void shellChanged();
    void surfaceChanged();
    void parentSurfaceChanged();
    void positionChanged();

private:
    void initialize() override;
};

QT_END_NAMESPACE

#endif  /*QWAYLANDXDGSHELL_H*/
