/****************************************************************************
**
** Copyright (C) 2014-2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef QWAYLANDSURFACE_H
#define QWAYLANDSURFACE_H

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>
#include <QtWaylandCompositor/qwaylandclient.h>

#include <QtCore/QScopedPointer>
#include <QtGui/QImage>
#include <QtGui/QWindow>
#include <QtCore/QVariantMap>

struct wl_client;
struct wl_resource;

QT_BEGIN_NAMESPACE

class QTouchEvent;
class QWaylandClient;
class QWaylandSurfacePrivate;
class QWaylandCompositor;
class QWaylandBufferRef;
class QWaylandView;
class QWaylandSurfaceOp;
class QWaylandInputMethodControl;
class QWaylandDrag;

class QWaylandSurfaceRole
{
public:
    QWaylandSurfaceRole(const QByteArray &n) : m_name(n) {}

    const QByteArray name() { return m_name; }

private:
    QByteArray m_name;
};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandSurface : public QWaylandObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandSurface)
    Q_PROPERTY(QWaylandClient *client READ client CONSTANT)
    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
    Q_PROPERTY(int bufferScale READ bufferScale NOTIFY bufferScaleChanged)
    Q_PROPERTY(Qt::ScreenOrientation contentOrientation READ contentOrientation NOTIFY contentOrientationChanged)
    Q_PROPERTY(QWaylandSurface::Origin origin READ origin NOTIFY originChanged)
    Q_PROPERTY(bool hasContent READ hasContent NOTIFY hasContentChanged)
    Q_PROPERTY(bool cursorSurface READ isCursorSurface WRITE markAsCursorSurface)

public:
    enum Origin {
        OriginTopLeft,
        OriginBottomLeft
    };
    Q_ENUM(Origin)

    QWaylandSurface();
    QWaylandSurface(QWaylandCompositor *compositor, QWaylandClient *client, uint id, int version);
    virtual ~QWaylandSurface();

    Q_INVOKABLE void initialize(QWaylandCompositor *compositor, QWaylandClient *client, uint id, int version);
    bool isInitialized() const;

    QWaylandClient *client() const;
    struct wl_client *waylandClient() const { return client()->client(); }

    bool setRole(QWaylandSurfaceRole *role, wl_resource *errorResource, uint32_t errorCode);
    QWaylandSurfaceRole *role() const;

    bool hasContent() const;

    QSize size() const;
    int bufferScale() const;

    Qt::ScreenOrientation contentOrientation() const;

    Origin origin() const;

    QWaylandCompositor *compositor() const;

    bool inputRegionContains(const QPoint &p) const;

    Q_INVOKABLE void destroy();
    Q_INVOKABLE bool isDestroyed() const;

    Q_INVOKABLE void frameStarted();
    Q_INVOKABLE void sendFrameCallbacks();

    QWaylandView *primaryView() const;
    void setPrimaryView(QWaylandView *view);

    QList<QWaylandView *> views() const;

    static QWaylandSurface *fromResource(::wl_resource *resource);
    struct wl_resource *resource() const;

    void markAsCursorSurface(bool cursorSurface);
    bool isCursorSurface() const;

    QWaylandInputMethodControl *inputMethodControl() const;

public Q_SLOTS:
    void updateSelection();

protected:
    QWaylandSurface(QWaylandSurfacePrivate &dptr);

Q_SIGNALS:
    void hasContentChanged();
    void damaged(const QRegion &rect);
    void parentChanged(QWaylandSurface *newParent, QWaylandSurface *oldParent);
    void childAdded(QWaylandSurface *child);
    void sizeChanged();
    void bufferScaleChanged();
    void offsetForNextFrame(const QPoint &offset);
    void contentOrientationChanged();
    void surfaceDestroyed();
    void originChanged();
    void subsurfacePositionChanged(const QPoint &position);
    void subsurfacePlaceAbove(QWaylandSurface *sibling);
    void subsurfacePlaceBelow(QWaylandSurface *sibling);
    void dragStarted(QWaylandDrag *drag);

    void configure(bool hasBuffer);
    void redraw();
};

QT_END_NAMESPACE

#endif // QWAYLANDSURFACE_H
