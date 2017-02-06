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

#include "qwlqttouch_p.h"
#include "qwaylandview.h"
#include <QTouchEvent>
#include <QWindow>

QT_BEGIN_NAMESPACE

namespace QtWayland {

static const int maxRawPos = 24;

TouchExtensionGlobal::TouchExtensionGlobal(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate(compositor)
    , QtWaylandServer::qt_touch_extension(compositor->display(), 1)
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

bool TouchExtensionGlobal::postTouchEvent(QTouchEvent *event, QWaylandSurface *surface)
{
    const QList<QTouchEvent::TouchPoint> points = event->touchPoints();
    const int pointCount = points.count();
    if (!pointCount)
        return false;

    wl_client *surfaceClient = surface->client()->client();
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

            int x = toFixed(tp.pos().x());
            int y = toFixed(tp.pos().y());
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

void TouchExtensionGlobal::setBehviorFlags(BehaviorFlags flags)
{
    if (m_flags == flags)
        return;

    m_flags = flags;
    behaviorFlagsChanged();
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
