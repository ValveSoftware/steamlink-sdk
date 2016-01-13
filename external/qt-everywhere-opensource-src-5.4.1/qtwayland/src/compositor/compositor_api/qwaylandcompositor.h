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

#ifndef QWAYLANDCOMPOSITOR_H
#define QWAYLANDCOMPOSITOR_H

#include <QtCompositor/qwaylandexport.h>

#include <QObject>
#include <QImage>
#include <QRect>

struct wl_display;

QT_BEGIN_NAMESPACE

class QMimeData;
class QUrl;
class QOpenGLContext;
class QWaylandSurface;
class QWaylandInputDevice;
class QWaylandInputPanel;
class QWaylandDrag;
class QWaylandGlobalInterface;
class QWaylandSurfaceView;

namespace QtWayland
{
    class Compositor;
}

class Q_COMPOSITOR_EXPORT QWaylandCompositor
{
public:
    enum ExtensionFlag {
        WindowManagerExtension = 0x01,
        OutputExtension = 0x02,
        SurfaceExtension = 0x04,
        QtKeyExtension = 0x08,
        TouchExtension = 0x10,
        SubSurfaceExtension = 0x20,
        TextInputExtension = 0x40,
        HardwareIntegrationExtension = 0x80,

        DefaultExtensions = WindowManagerExtension | OutputExtension | SurfaceExtension | QtKeyExtension | TouchExtension | HardwareIntegrationExtension
    };
    Q_DECLARE_FLAGS(ExtensionFlags, ExtensionFlag)

    QWaylandCompositor(QWindow *window = 0, const char *socketName = 0, ExtensionFlags extensions = DefaultExtensions);
    virtual ~QWaylandCompositor();

    void addGlobalInterface(QWaylandGlobalInterface *interface);
    void addDefaultShell();
    ::wl_display *waylandDisplay() const;

    void frameStarted();
    void sendFrameCallbacks(QList<QWaylandSurface *> visibleSurfaces);

    void destroyClientForSurface(QWaylandSurface *surface);
    void destroyClient(WaylandClient *client);

    QList<QWaylandSurface *> surfacesForClient(WaylandClient* client) const;
    QList<QWaylandSurface *> surfaces() const;

    QWindow *window()const;

    virtual void surfaceCreated(QWaylandSurface *surface) = 0;
    virtual void surfaceAboutToBeDestroyed(QWaylandSurface *surface);

    virtual QWaylandSurfaceView *pickView(const QPointF &globalPosition) const;
    virtual QPointF mapToView(QWaylandSurfaceView *view, const QPointF &surfacePosition) const;

    virtual bool openUrl(WaylandClient *client, const QUrl &url);

    QtWayland::Compositor *handle() const;

    void setRetainedSelectionEnabled(bool enabled);
    bool retainedSelectionEnabled() const;
    void overrideSelection(const QMimeData *data);

    void setClientFullScreenHint(bool value);

    const char *socketName() const;

    void setScreenOrientation(Qt::ScreenOrientation orientation);

    void setOutputGeometry(const QRect &outputGeometry);
    QRect outputGeometry() const;

    void setOutputRefreshRate(int refreshRate);
    int outputRefreshRate() const;

    QWaylandInputDevice *defaultInputDevice() const;

    QWaylandInputPanel *inputPanel() const;
    QWaylandDrag *drag() const;

    bool isDragging() const;
    void sendDragMoveEvent(const QPoint &global, const QPoint &local, QWaylandSurface *surface);
    void sendDragEndEvent();

    virtual void setCursorSurface(QWaylandSurface *surface, int hotspotX, int hotspotY);

    void cleanupGraphicsResources();

    enum TouchExtensionFlag {
        TouchExtMouseFromTouch = 0x01
    };
    Q_DECLARE_FLAGS(TouchExtensionFlags, TouchExtensionFlag)
    void configureTouchExtension(TouchExtensionFlags flags);

    virtual QWaylandSurfaceView *createView(QWaylandSurface *surface);

protected:
    QWaylandCompositor(QWindow *window, const char *socketName, QtWayland::Compositor *dptr);
    virtual void retainedSelectionReceived(QMimeData *mimeData);

    friend class QtWayland::Compositor;
    QtWayland::Compositor *m_compositor;

private:
    QWindow  *m_toplevel_window;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QWaylandCompositor::ExtensionFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QWaylandCompositor::TouchExtensionFlags)

QT_END_NAMESPACE

#endif // QWAYLANDCOMPOSITOR_H
