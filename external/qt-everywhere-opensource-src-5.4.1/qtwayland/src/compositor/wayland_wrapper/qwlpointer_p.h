/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
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

#ifndef QTWAYLAND_QWLPOINTER_P_H
#define QTWAYLAND_QWLPOINTER_P_H

#include <QtCompositor/qwaylandexport.h>

#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QObject>

#include <QtCompositor/private/qwayland-server-wayland.h>

#include <stdint.h>

#include "qwllistener_p.h"

QT_BEGIN_NAMESPACE

class QWaylandSurfaceView;

namespace QtWayland {

class Compositor;
class InputDevice;
class Pointer;
class Surface;

class Q_COMPOSITOR_EXPORT PointerGrabber {
public:
    virtual ~PointerGrabber();

    virtual void focus() = 0;
    virtual void motion(uint32_t time) = 0;
    virtual void button(uint32_t time, Qt::MouseButton button, uint32_t state) = 0;

    Pointer *m_pointer;
};

class Q_COMPOSITOR_EXPORT Pointer : public QObject, public QtWaylandServer::wl_pointer, public PointerGrabber
{
public:
    Pointer(Compositor *compositor, InputDevice *seat);

    void setFocus(QWaylandSurfaceView *surface, const QPointF &position);

    void startGrab(PointerGrabber *currentGrab);
    void endGrab();
    PointerGrabber *currentGrab() const;
    Qt::MouseButton grabButton() const;
    uint32_t grabTime() const;
    uint32_t grabSerial() const;

    void setCurrent(QWaylandSurfaceView *surface, const QPointF &point);
    void setMouseFocus(QWaylandSurfaceView *surface, const QPointF &localPos, const QPointF &globalPos);

    void sendButton(uint32_t time, Qt::MouseButton button, uint32_t state);

    void sendMousePressEvent(Qt::MouseButton button, const QPointF &localPos, const QPointF &globalPos);
    void sendMouseReleaseEvent(Qt::MouseButton button, const QPointF &localPos, const QPointF &globalPos);
    void sendMouseMoveEvent(const QPointF &localPos, const QPointF &globalPos);
    void sendMouseWheelEvent(Qt::Orientation orientation, int delta);

    QWaylandSurfaceView *focusSurface() const;
    QWaylandSurfaceView *current() const;
    QPointF position() const;
    QPointF currentPosition() const;
    Resource *focusResource() const;

    bool buttonPressed() const;

    void focus() Q_DECL_OVERRIDE;
    void motion(uint32_t time) Q_DECL_OVERRIDE;
    void button(uint32_t time, Qt::MouseButton button, uint32_t state) Q_DECL_OVERRIDE;

protected:
    void pointer_set_cursor(Resource *resource, uint32_t serial, wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y) Q_DECL_OVERRIDE;
    void pointer_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;

private:
    void focusDestroyed(void *data);

    Compositor *m_compositor;
    InputDevice *m_seat;

    PointerGrabber *m_grab;
    Qt::MouseButton m_grabButton;
    uint32_t m_grabTime;
    uint32_t m_grabSerial;

    QPointF m_position;

    QWaylandSurfaceView *m_focus;
    Resource *m_focusResource;

    QWaylandSurfaceView *m_current;
    QPointF m_currentPoint;

    int m_buttonCount;

    WlListener m_focusDestroyListener;
};

} // namespace QtWayland

QT_END_NAMESPACE

#endif // QTWAYLAND_QWLPOINTER_P_H
