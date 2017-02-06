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

#ifndef QWAYLANDSEAT_P_H
#define QWAYLANDSEAT_P_H

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

#include <stdint.h>

#include <QtWaylandCompositor/private/qtwaylandcompositorglobal_p.h>
#include <QtWaylandCompositor/qwaylandseat.h>

#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QScopedPointer>
#include <QtCore/private/qobject_p.h>

#include <QtWaylandCompositor/private/qwayland-server-wayland.h>

QT_BEGIN_NAMESPACE

class QKeyEvent;
class QTouchEvent;
class QWaylandSeat;
class QWaylandDrag;
class QWaylandView;

namespace QtWayland {

class Compositor;
class DataDevice;
class Surface;
class DataDeviceManager;
class Pointer;
class Keyboard;
class Touch;
class InputMethod;

}

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandSeatPrivate : public QObjectPrivate, public QtWaylandServer::wl_seat
{
public:
    Q_DECLARE_PUBLIC(QWaylandSeat)

    QWaylandSeatPrivate(QWaylandSeat *seat);
    ~QWaylandSeatPrivate();

    void clientRequestedDataDevice(QtWayland::DataDeviceManager *dndSelection, struct wl_client *client, uint32_t id);
    void setCapabilities(QWaylandSeat::CapabilityFlags caps);

    static QWaylandSeatPrivate *get(QWaylandSeat *device) { return device->d_func(); }

    QtWayland::DataDevice *dataDevice() const { return data_device.data(); }

protected:
    void seat_bind_resource(wl_seat::Resource *resource) Q_DECL_OVERRIDE;

    void seat_get_pointer(wl_seat::Resource *resource,
                          uint32_t id) Q_DECL_OVERRIDE;
    void seat_get_keyboard(wl_seat::Resource *resource,
                           uint32_t id) Q_DECL_OVERRIDE;
    void seat_get_touch(wl_seat::Resource *resource,
                        uint32_t id) Q_DECL_OVERRIDE;

    void seat_destroy_resource(wl_seat::Resource *resource) Q_DECL_OVERRIDE;

private:
    bool isInitialized;
    QWaylandCompositor *compositor;
    QWaylandView *mouseFocus;
    QWaylandSurface *keyboardFocus;
    QWaylandSeat::CapabilityFlags capabilities;

    QScopedPointer<QWaylandPointer> pointer;
    QScopedPointer<QWaylandKeyboard> keyboard;
    QScopedPointer<QWaylandTouch> touch;
    QScopedPointer<QtWayland::DataDevice> data_device;
    QScopedPointer<QWaylandDrag> drag_handle;
    QScopedPointer<QWaylandKeymap> keymap;

};

QT_END_NAMESPACE

#endif // QWAYLANDSEAT_P_H
