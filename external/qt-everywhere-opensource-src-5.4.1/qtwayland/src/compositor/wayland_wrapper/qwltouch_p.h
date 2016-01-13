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

#ifndef QTWAYLAND_QWLTOUCH_P_H
#define QTWAYLAND_QWLTOUCH_P_H

#include <QtCompositor/qwaylandexport.h>

#include <QtCore/QPoint>
#include <QtCore/QObject>

#include <QtCompositor/private/qwayland-server-wayland.h>

#include "qwllistener_p.h"

QT_BEGIN_NAMESPACE

class QWaylandSurfaceView;

namespace QtWayland {

class Compositor;
class Surface;
class Touch;

class Q_COMPOSITOR_EXPORT TouchGrabber {
public:
    TouchGrabber();
    virtual ~TouchGrabber();

    virtual void down(uint32_t time, int touch_id, const QPointF &position) = 0;
    virtual void up(uint32_t time, int touch_id) = 0;
    virtual void motion(uint32_t time, int touch_id, const QPointF &position) = 0;

    const Touch *touch() const;
    Touch *touch();
    void setTouch(Touch *touch);

private:
    Touch *m_touch;
};

class Q_COMPOSITOR_EXPORT Touch : public QObject, public QtWaylandServer::wl_touch, public TouchGrabber
{
public:
    explicit Touch(Compositor *compositor);

    void setFocus(QWaylandSurfaceView *surface);

    void startGrab(TouchGrabber *grab);
    void endGrab();

    void sendCancel();
    void sendFrame();

    void sendDown(int touch_id, const QPointF &position);
    void sendMotion(int touch_id, const QPointF &position);
    void sendUp(int touch_id);

    void down(uint32_t time, int touch_id, const QPointF &position);
    void up(uint32_t time, int touch_id);
    void motion(uint32_t time, int touch_id, const QPointF &position);

private:
    void focusDestroyed(void *data);

    Compositor *m_compositor;

    QWaylandSurfaceView *m_focus;
    Resource *m_focusResource;
    WlListener m_focusDestroyListener;

    TouchGrabber *m_grab;
};

} // namespace QtWayland

QT_END_NAMESPACE

#endif // QTWAYLAND_QWLTOUCH_P_H
