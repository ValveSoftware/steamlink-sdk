/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QWAYLANDINPUTDEVICE_H
#define QWAYLANDINPUTDEVICE_H

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

#include <QtWaylandClient/private/qtwaylandclientglobal_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include <QSocketNotifier>
#include <QObject>
#include <QTimer>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformscreen.h>
#include <qpa/qwindowsysteminterface.h>

#include <wayland-client.h>

#include <QtWaylandClient/private/qwayland-wayland.h>

#if QT_CONFIG(xkbcommon_evdev)
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#endif

#include <QtCore/QDebug>

struct wl_cursor_image;

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandWindow;
class QWaylandDisplay;
class QWaylandDataDevice;
class QWaylandTextInput;

class Q_WAYLAND_CLIENT_EXPORT QWaylandInputDevice
                            : public QObject
                            , public QtWayland::wl_seat
{
    Q_OBJECT
public:
    class Keyboard;
    class Pointer;
    class Touch;

    QWaylandInputDevice(QWaylandDisplay *display, int version, uint32_t id);
    ~QWaylandInputDevice();

    uint32_t capabilities() const { return mCaps; }

    struct ::wl_seat *wl_seat() { return QtWayland::wl_seat::object(); }

    void setCursor(const QCursor &cursor, QWaylandScreen *screen);
    void setCursor(struct wl_buffer *buffer, struct ::wl_cursor_image *image);
    void setCursor(struct wl_buffer *buffer, const QPoint &hotSpot, const QSize &size);
    void setCursor(const QSharedPointer<QWaylandBuffer> &buffer, const QPoint &hotSpot);
    void handleWindowDestroyed(QWaylandWindow *window);
    void handleEndDrag();

    void setDataDevice(QWaylandDataDevice *device);
    QWaylandDataDevice *dataDevice() const;

    void setTextInput(QWaylandTextInput *textInput);
    QWaylandTextInput *textInput() const;

    void removeMouseButtonFromState(Qt::MouseButton button);

    QWaylandWindow *pointerFocus() const;
    QWaylandWindow *keyboardFocus() const;
    QWaylandWindow *touchFocus() const;

    Qt::KeyboardModifiers modifiers() const;

    uint32_t serial() const;
    uint32_t cursorSerial() const;

    virtual Keyboard *createKeyboard(QWaylandInputDevice *device);
    virtual Pointer *createPointer(QWaylandInputDevice *device);
    virtual Touch *createTouch(QWaylandInputDevice *device);

private:
    void setCursor(Qt::CursorShape cursor, QWaylandScreen *screen);

    QWaylandDisplay *mQDisplay;
    struct wl_display *mDisplay;

    int mVersion;
    uint32_t mCaps;

    struct wl_surface *pointerSurface;

    QWaylandDataDevice *mDataDevice;

    Keyboard *mKeyboard;
    Pointer *mPointer;
    Touch *mTouch;

    QWaylandTextInput *mTextInput;

    uint32_t mTime;
    uint32_t mSerial;

    void seat_capabilities(uint32_t caps) Q_DECL_OVERRIDE;
    void handleTouchPoint(int id, double x, double y, Qt::TouchPointState state);

    QTouchDevice *mTouchDevice;

    QSharedPointer<QWaylandBuffer> mPixmapCursor;

    friend class QWaylandTouchExtension;
    friend class QWaylandQtKeyExtension;
};

inline uint32_t QWaylandInputDevice::serial() const
{
    return mSerial;
}


class Q_WAYLAND_CLIENT_EXPORT QWaylandInputDevice::Keyboard : public QObject, public QtWayland::wl_keyboard
{
    Q_OBJECT

public:
    Keyboard(QWaylandInputDevice *p);
    virtual ~Keyboard();

    void stopRepeat();

    void keyboard_keymap(uint32_t format,
                         int32_t fd,
                         uint32_t size) Q_DECL_OVERRIDE;
    void keyboard_enter(uint32_t time,
                        struct wl_surface *surface,
                        struct wl_array *keys) Q_DECL_OVERRIDE;
    void keyboard_leave(uint32_t time,
                        struct wl_surface *surface) Q_DECL_OVERRIDE;
    void keyboard_key(uint32_t serial, uint32_t time,
                      uint32_t key, uint32_t state) Q_DECL_OVERRIDE;
    void keyboard_modifiers(uint32_t serial,
                            uint32_t mods_depressed,
                            uint32_t mods_latched,
                            uint32_t mods_locked,
                            uint32_t group) Q_DECL_OVERRIDE;

    QWaylandInputDevice *mParent;
    QWaylandWindow *mFocus;
#if QT_CONFIG(xkbcommon_evdev)
    xkb_context *mXkbContext;
    xkb_keymap *mXkbMap;
    xkb_state *mXkbState;
#endif
    uint32_t mNativeModifiers;

    int mRepeatKey;
    uint32_t mRepeatCode;
    uint32_t mRepeatTime;
    QString mRepeatText;
#if QT_CONFIG(xkbcommon_evdev)
    xkb_keysym_t mRepeatSym;
#endif
    QTimer mRepeatTimer;

    Qt::KeyboardModifiers modifiers() const;

private slots:
    void repeatKey();

private:
#if QT_CONFIG(xkbcommon_evdev)
    bool createDefaultKeyMap();
    void releaseKeyMap();
#endif

};

class Q_WAYLAND_CLIENT_EXPORT QWaylandInputDevice::Pointer : public QtWayland::wl_pointer
{

public:
    Pointer(QWaylandInputDevice *p);
    virtual ~Pointer();

    void pointer_enter(uint32_t serial, struct wl_surface *surface,
                       wl_fixed_t sx, wl_fixed_t sy) Q_DECL_OVERRIDE;
    void pointer_leave(uint32_t time, struct wl_surface *surface) Q_DECL_OVERRIDE;
    void pointer_motion(uint32_t time,
                        wl_fixed_t sx, wl_fixed_t sy) Q_DECL_OVERRIDE;
    void pointer_button(uint32_t serial, uint32_t time,
                        uint32_t button, uint32_t state) Q_DECL_OVERRIDE;
    void pointer_axis(uint32_t time,
                      uint32_t axis,
                      wl_fixed_t value) Q_DECL_OVERRIDE;

    void releaseButtons();

    QWaylandInputDevice *mParent;
    QWaylandWindow *mFocus;
    uint32_t mEnterSerial;
    uint32_t mCursorSerial;
    QPointF mSurfacePos;
    QPointF mGlobalPos;
    Qt::MouseButtons mButtons;
    wl_buffer *mCursorBuffer;
    Qt::CursorShape mCursorShape;
};

class Q_WAYLAND_CLIENT_EXPORT QWaylandInputDevice::Touch : public QtWayland::wl_touch
{
public:
    Touch(QWaylandInputDevice *p);
    virtual ~Touch();

    void touch_down(uint32_t serial,
                    uint32_t time,
                    struct wl_surface *surface,
                    int32_t id,
                    wl_fixed_t x,
                    wl_fixed_t y) Q_DECL_OVERRIDE;
    void touch_up(uint32_t serial,
                  uint32_t time,
                  int32_t id) Q_DECL_OVERRIDE;
    void touch_motion(uint32_t time,
                      int32_t id,
                      wl_fixed_t x,
                      wl_fixed_t y) Q_DECL_OVERRIDE;
    void touch_frame() Q_DECL_OVERRIDE;
    void touch_cancel() Q_DECL_OVERRIDE;

    bool allTouchPointsReleased();
    void releasePoints();

    QWaylandInputDevice *mParent;
    QWaylandWindow *mFocus;
    QList<QWindowSystemInterface::TouchPoint> mTouchPoints;
    QList<QWindowSystemInterface::TouchPoint> mPrevTouchPoints;
};

class QWaylandPointerEvent
{
public:
    enum Type {
        Enter,
        Motion,
        Wheel
    };
    inline QWaylandPointerEvent(Type t, ulong ts, const QPointF &l, const QPointF &g, Qt::MouseButtons b, Qt::KeyboardModifiers m)
        : type(t)
        , timestamp(ts)
        , local(l)
        , global(g)
        , buttons(b)
        , modifiers(m)
    {}
    inline QWaylandPointerEvent(Type t, ulong ts, const QPointF &l, const QPointF &g, const QPoint &pd, const QPoint &ad)
        : type(t)
        , timestamp(ts)
        , local(l)
        , global(g)
        , pixelDelta(pd)
        , angleDelta(ad)
    {}

    Type type;
    ulong timestamp;
    QPointF local;
    QPointF global;
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;
    QPoint pixelDelta;
    QPoint angleDelta;
};

}

QT_END_NAMESPACE

#endif
