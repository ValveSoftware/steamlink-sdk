/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDTOUCH_H
#define QWAYLANDTOUCH_H

#include <qpa/qwindowsysteminterface.h>

#include <QtWaylandClient/private/qwayland-touch-extension.h>
#include <QtWaylandClient/private/qwaylandclientexport_p.h>

QT_BEGIN_NAMESPACE

class QWaylandDisplay;
class QWaylandInputDevice;

class Q_WAYLAND_CLIENT_EXPORT QWaylandTouchExtension : public QtWayland::qt_touch_extension
{
public:
    QWaylandTouchExtension(QWaylandDisplay *display, uint32_t id);

    void touchCanceled();

private:
    void registerDevice(int caps);

    QWaylandDisplay *mDisplay;

    void touch_extension_touch(uint32_t time,
                               uint32_t id,
                               uint32_t state,
                               int32_t x,
                               int32_t y,
                               int32_t normalized_x,
                               int32_t normalized_y,
                               int32_t width,
                               int32_t height,
                               uint32_t pressure,
                               int32_t velocity_x,
                               int32_t velocity_y,
                               uint32_t flags,
                               struct wl_array *rawdata) Q_DECL_OVERRIDE;
    void touch_extension_configure(uint32_t flags) Q_DECL_OVERRIDE;

    void sendTouchEvent();

    QList<QWindowSystemInterface::TouchPoint> mTouchPoints;
    QList<QWindowSystemInterface::TouchPoint> mPrevTouchPoints;
    QTouchDevice *mTouchDevice;
    uint32_t mTimestamp;
    int mPointsLeft;
    uint32_t mFlags;
    int mMouseSourceId;
    QPointF mLastMouseLocal;
    QPointF mLastMouseGlobal;
    QWindow *mTargetWindow;
    QWaylandInputDevice *mInputDevice;
};

QT_END_NAMESPACE

#endif // QWAYLANDTOUCH_H
