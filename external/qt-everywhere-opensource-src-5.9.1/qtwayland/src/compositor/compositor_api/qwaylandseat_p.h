/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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

    void setCapabilities(QWaylandSeat::CapabilityFlags caps);

    static QWaylandSeatPrivate *get(QWaylandSeat *device) { return device->d_func(); }

#if QT_CONFIG(wayland_datadevice)
    void clientRequestedDataDevice(QtWayland::DataDeviceManager *dndSelection, struct wl_client *client, uint32_t id);
    QtWayland::DataDevice *dataDevice() const { return data_device.data(); }
#endif

protected:
    void seat_bind_resource(wl_seat::Resource *resource) override;

    void seat_get_pointer(wl_seat::Resource *resource,
                          uint32_t id) override;
    void seat_get_keyboard(wl_seat::Resource *resource,
                           uint32_t id) override;
    void seat_get_touch(wl_seat::Resource *resource,
                        uint32_t id) override;

    void seat_destroy_resource(wl_seat::Resource *resource) override;

private:
    bool isInitialized;
    QWaylandCompositor *compositor;
    QWaylandView *mouseFocus;
    QWaylandSurface *keyboardFocus;
    QWaylandSeat::CapabilityFlags capabilities;

    QScopedPointer<QWaylandPointer> pointer;
    QScopedPointer<QWaylandKeyboard> keyboard;
    QScopedPointer<QWaylandTouch> touch;
#if QT_CONFIG(wayland_datadevice)
    QScopedPointer<QtWayland::DataDevice> data_device;
# if QT_CONFIG(draganddrop)
    QScopedPointer<QWaylandDrag> drag_handle;
# endif
#endif
    QScopedPointer<QWaylandKeymap> keymap;

};

QT_END_NAMESPACE

#endif // QWAYLANDSEAT_P_H
