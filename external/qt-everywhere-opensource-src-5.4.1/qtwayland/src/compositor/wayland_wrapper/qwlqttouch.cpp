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

#include "qwlqttouch_p.h"
#include "qwlsurface_p.h"
#include "qwaylandsurfaceview.h"
#include <QTouchEvent>
#include <QWindow>

QT_BEGIN_NAMESPACE

namespace QtWayland {

static const int maxRawPos = 24;

TouchExtensionGlobal::TouchExtensionGlobal(Compositor *compositor)
    : QtWaylandServer::qt_touch_extension(compositor->wl_display(), 1)
    , m_compositor(compositor)
    , m_flags(0)
    , m_resources()
    , m_posData(maxRawPos * 2)
{
}

TouchExtensionGlobal::~TouchExtensionGlobal()
{
}

static inline int toFixed(qreal f)
{
    return int(f * 10000);
}

bool TouchExtensionGlobal::postTouchEvent(QTouchEvent *event, QWaylandSurfaceView *view)
{
    const QList<QTouchEvent::TouchPoint> points = event->touchPoints();
    const int pointCount = points.count();
    if (!pointCount)
        return false;

    QPointF surfacePos = view->pos();
    wl_client *surfaceClient = view->surface()->handle()->resource()->client();
    uint32_t time = m_compositor->currentTimeMsecs();
    const int rescount = m_resources.count();

    for (int res = 0; res < rescount; ++res) {
        Resource *target = m_resources.at(res);
        if (target->client() != surfaceClient)
            continue;

        // We will use no touch_frame type of event, to reduce the number of
        // events flowing through the wire. Instead, the number of points sent is
        // included in the touch point events.
        int sentPointCount = 0;
        for (int i = 0; i < pointCount; ++i) {
            if (points.at(i).state() != Qt::TouchPointStationary)
                ++sentPointCount;
        }

        for (int i = 0; i < pointCount; ++i) {
            const QTouchEvent::TouchPoint &tp(points.at(i));
            // Stationary points are never sent. They are cached on client side.
            if (tp.state() == Qt::TouchPointStationary)
                continue;

            uint32_t id = tp.id();
            uint32_t state = (tp.state() & 0xFFFF) | (sentPointCount << 16);
            uint32_t flags = (tp.flags() & 0xFFFF) | (int(event->device()->capabilities()) << 16);

            QPointF p = tp.pos() - surfacePos; // surface-relative
            int x = toFixed(p.x());
            int y = toFixed(p.y());
            int nx = toFixed(tp.normalizedPos().x());
            int ny = toFixed(tp.normalizedPos().y());
            int w = toFixed(tp.rect().width());
            int h = toFixed(tp.rect().height());
            int vx = toFixed(tp.velocity().x());
            int vy = toFixed(tp.velocity().y());
            uint32_t pressure = uint32_t(tp.pressure() * 255);

            QByteArray rawData;
            QVector<QPointF> rawPosList = tp.rawScreenPositions();
            int rawPosCount = rawPosList.count();
            if (rawPosCount) {
                rawPosCount = qMin(maxRawPos, rawPosCount);
                QVector<float>::iterator iter = m_posData.begin();
                for (int rpi = 0; rpi < rawPosCount; ++rpi) {
                    const QPointF &rawPos(rawPosList.at(rpi));
                    // This will stay in screen coordinates for performance
                    // reasons, clients using this data will presumably know
                    // what they are doing.
                    *iter++ = static_cast<float>(rawPos.x());
                    *iter++ = static_cast<float>(rawPos.y());
                }
                rawData = QByteArray::fromRawData(reinterpret_cast<const char*>(m_posData.constData()), sizeof(float) * rawPosCount * 2);
            }

            send_touch(target->handle,
                       time, id, state,
                       x, y, nx, ny, w, h,
                       pressure, vx, vy,
                       flags, rawData);
        }

        return true;
    }

    return false;
}

void TouchExtensionGlobal::touch_extension_bind_resource(Resource *resource)
{
    m_resources.append(resource);
    send_configure(resource->handle, m_flags);
}

void TouchExtensionGlobal::touch_extension_destroy_resource(Resource *resource)
{
    m_resources.removeOne(resource);
}

}

QT_END_NAMESPACE
