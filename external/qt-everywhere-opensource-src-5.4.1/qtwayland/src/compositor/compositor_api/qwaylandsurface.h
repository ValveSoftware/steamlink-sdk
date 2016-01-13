/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDSURFACE_H
#define QWAYLANDSURFACE_H

#include <QtCompositor/qwaylandexport.h>

#include <QtCore/QScopedPointer>
#include <QtGui/QImage>
#include <QtGui/QWindow>
#include <QtCore/QVariantMap>

struct wl_client;
struct wl_resource;

QT_BEGIN_NAMESPACE

class QTouchEvent;
class QWaylandSurfacePrivate;
class QWaylandCompositor;
class QWaylandBufferRef;
class QWaylandSurfaceView;
class QWaylandSurfaceInterface;
class QWaylandSurfaceOp;

namespace QtWayland {
class Surface;
class SurfacePrivate;
class ExtendedSurface;
}

class Q_COMPOSITOR_EXPORT QWaylandBufferAttacher
{
public:
    virtual ~QWaylandBufferAttacher() {}

protected:
    virtual void attach(const QWaylandBufferRef &ref) = 0;

    friend class QtWayland::Surface;
};

class Q_COMPOSITOR_EXPORT QWaylandSurface : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandSurface)
    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
    Q_PROPERTY(QWaylandSurface::WindowFlags windowFlags READ windowFlags NOTIFY windowFlagsChanged)
    Q_PROPERTY(QWaylandSurface::WindowType windowType READ windowType NOTIFY windowTypeChanged)
    Q_PROPERTY(Qt::ScreenOrientation contentOrientation READ contentOrientation NOTIFY contentOrientationChanged)
    Q_PROPERTY(QString className READ className NOTIFY classNameChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(Qt::ScreenOrientations orientationUpdateMask READ orientationUpdateMask NOTIFY orientationUpdateMaskChanged)
    Q_PROPERTY(QWindow::Visibility visibility READ visibility WRITE setVisibility NOTIFY visibilityChanged)
    Q_PROPERTY(QWaylandSurface *transientParent READ transientParent)
    Q_PROPERTY(QPointF transientOffset READ transientOffset)

    Q_ENUMS(WindowFlag WindowType)
    Q_FLAGS(WindowFlag WindowFlags)

public:
    enum WindowFlag {
        OverridesSystemGestures     = 0x0001,
        StaysOnTop                  = 0x0002,
        BypassWindowManager         = 0x0004
    };
    Q_DECLARE_FLAGS(WindowFlags, WindowFlag)

    enum WindowType {
        None,
        Toplevel,
        Transient,
        Popup
    };

    enum Type {
        Invalid,
        Shm,
        Texture
    };

    QWaylandSurface(wl_client *client, quint32 id, int version, QWaylandCompositor *compositor);
    virtual ~QWaylandSurface();

    WaylandClient *client() const;

    QWaylandSurface *parentSurface() const;
    QLinkedList<QWaylandSurface *> subSurfaces() const;
    void addInterface(QWaylandSurfaceInterface *interface);
    void removeInterface(QWaylandSurfaceInterface *interface);

    Type type() const;
    bool isYInverted() const;

    bool visible() const;
    bool isMapped() const;

    QSize size() const;
    Q_INVOKABLE void requestSize(const QSize &size);

    Qt::ScreenOrientations orientationUpdateMask() const;
    Qt::ScreenOrientation contentOrientation() const;

    WindowFlags windowFlags() const;

    WindowType windowType() const;

    QWindow::Visibility visibility() const;
    void setVisibility(QWindow::Visibility visibility);
    Q_INVOKABLE void sendOnScreenVisibilityChange(bool visible); // Compat

    QWaylandSurface *transientParent() const;

    QPointF transientOffset() const;

    QtWayland::Surface *handle();

    qint64 processId() const;
    QByteArray authenticationToken() const;
    QVariantMap windowProperties() const;
    void setWindowProperty(const QString &name, const QVariant &value);

    QWaylandCompositor *compositor() const;

    QString className() const;

    QString title() const;

    bool hasInputPanelSurface() const;

    bool transientInactive() const;

    Q_INVOKABLE void destroy();
    Q_INVOKABLE void destroySurface();
    Q_INVOKABLE void ping();

    void ref();
    void setMapped(bool mapped);

    void setBufferAttacher(QWaylandBufferAttacher *attacher);
    QWaylandBufferAttacher *bufferAttacher() const;

    QList<QWaylandSurfaceView *> views() const;
    QList<QWaylandSurfaceInterface *> interfaces() const;

    bool sendInterfaceOp(QWaylandSurfaceOp &op);

    static QWaylandSurface *fromResource(::wl_resource *resource);

public slots:
    void updateSelection();

protected:
    QWaylandSurface(QWaylandSurfacePrivate *dptr);

Q_SIGNALS:
    void mapped();
    void unmapped();
    void damaged(const QRegion &rect);
    void parentChanged(QWaylandSurface *newParent, QWaylandSurface *oldParent);
    void sizeChanged();
    void windowPropertyChanged(const QString &name, const QVariant &value);
    void windowFlagsChanged(WindowFlags flags);
    void windowTypeChanged(WindowType type);
    void contentOrientationChanged();
    void orientationUpdateMaskChanged();
    void extendedSurfaceReady();
    void classNameChanged();
    void titleChanged();
    void raiseRequested();
    void lowerRequested();
    void visibilityChanged();
    void pong();
    void surfaceDestroyed();

    void configure(bool hasBuffer);
    void redraw();

    friend class QWaylandSurfaceView;
    friend class QWaylandSurfaceInterface;
};

QT_END_NAMESPACE

#endif // QWAYLANDSURFACE_H
