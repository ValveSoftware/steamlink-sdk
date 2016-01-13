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

#include "qwaylandinputdevice_p.h"

#include "qwaylandintegration_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandbuffer_p.h"
#include "qwaylanddatadevice_p.h"
#include "qwaylanddatadevicemanager_p.h"
#include "qwaylandtouch_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandcursor_p.h"
#include "qwaylanddisplay_p.h"

#include <QtGui/private/qpixmap_raster_p.h>
#include <qpa/qplatformwindow.h>
#include <QDebug>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <wayland-cursor.h>

#include <QtGui/QGuiApplication>

#ifndef QT_NO_WAYLAND_XKB
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#endif

QT_BEGIN_NAMESPACE

class QWaylandInputDevice::Keyboard : public QtWayland::wl_keyboard
{
public:
    Keyboard(QWaylandInputDevice *p)
        : mParent(p)
        , mFocus(0)
#ifndef QT_NO_WAYLAND_XKB
        , mXkbContext(0)
        , mXkbMap(0)
        , mXkbState(0)
#endif
        , mFocusCallback(0)
        , mNativeModifiers(0)
    {
#ifndef QT_NO_WAYLAND_XKB
        xkb_rule_names names;
        names.rules = strdup("evdev");
        names.model = strdup("pc105");
        names.layout = strdup("us");
        names.variant = strdup("");
        names.options = strdup("");

        mXkbContext = xkb_context_new(xkb_context_flags(0));
        if (mXkbContext) {
            mXkbMap = xkb_map_new_from_names(mXkbContext, &names, xkb_map_compile_flags(0));
            if (mXkbMap) {
                mXkbState = xkb_state_new(mXkbMap);
            }
        }

        if (!mXkbContext || !mXkbMap || !mXkbState)
            qWarning() << "xkb_map_new_from_names failed, no key input";
#endif
    }
    ~Keyboard()
    {
#ifndef QT_NO_WAYLAND_XKB
        if (mXkbState)
            xkb_state_unref(mXkbState);
        if (mXkbMap)
            xkb_map_unref(mXkbMap);
        if (mXkbContext)
            xkb_context_unref(mXkbContext);
#endif
        wl_keyboard_destroy(object());
    }

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
#ifndef QT_NO_WAYLAND_XKB
    xkb_context *mXkbContext;
    xkb_keymap *mXkbMap;
    xkb_state *mXkbState;
#endif
    struct wl_callback *mFocusCallback;
    uint32_t mNativeModifiers;

    int mRepeatKey;
    uint32_t mRepeatCode;
    uint32_t mRepeatTime;
    QString mRepeatText;
#ifndef QT_NO_WAYLAND_XKB
    xkb_keysym_t mRepeatSym;
#endif

    static const wl_callback_listener callback;
    static void focusCallback(void *data, struct wl_callback *callback, uint32_t time);
};

class QWaylandInputDevice::Pointer : public QtWayland::wl_pointer
{
public:
    Pointer(QWaylandInputDevice *p)
        : mParent(p)
        , mFocus(0)
        , mEnterSerial(0)
        , mCursorSerial(0)
        , mButtons(0)
    {
    }
    ~Pointer()
    {
        wl_pointer_destroy(object());
    }

    void pointer_enter(uint32_t serial, struct wl_surface *surface,
                       wl_fixed_t sx, wl_fixed_t sy) Q_DECL_OVERRIDE;
    void pointer_leave(uint32_t time, struct wl_surface *surface);
    void pointer_motion(uint32_t time,
                        wl_fixed_t sx, wl_fixed_t sy) Q_DECL_OVERRIDE;
    void pointer_button(uint32_t serial, uint32_t time,
                        uint32_t button, uint32_t state) Q_DECL_OVERRIDE;
    void pointer_axis(uint32_t time,
                      uint32_t axis,
                      wl_fixed_t value) Q_DECL_OVERRIDE;

    QWaylandInputDevice *mParent;
    QWaylandWindow *mFocus;
    uint32_t mEnterSerial;
    uint32_t mCursorSerial;
    QPointF mSurfacePos;
    QPointF mGlobalPos;
    Qt::MouseButtons mButtons;
};

class QWaylandInputDevice::Touch : public QtWayland::wl_touch
{
public:
    Touch(QWaylandInputDevice *p)
        : mParent(p)
        , mFocus(0)
    {
    }
    ~Touch()
    {
        wl_touch_destroy(object());
    }

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

    QWaylandInputDevice *mParent;
    QWaylandWindow *mFocus;
    QList<QWindowSystemInterface::TouchPoint> mTouchPoints;
    QList<QWindowSystemInterface::TouchPoint> mPrevTouchPoints;
};

QWaylandInputDevice::QWaylandInputDevice(QWaylandDisplay *display, int version, uint32_t id)
    : QObject()
    , QtWayland::wl_seat(display->wl_registry(), id, qMin(version, 2))
    , mQDisplay(display)
    , mDisplay(display->wl_display())
    , mCaps(0)
    , mDataDevice(0)
    , mKeyboard(0)
    , mPointer(0)
    , mTouch(0)
    , mTime(0)
    , mSerial(0)
    , mTouchDevice(0)
{
    if (mQDisplay->dndSelectionHandler()) {
        mDataDevice = mQDisplay->dndSelectionHandler()->getDataDevice(this);
    }

    connect(&mRepeatTimer, SIGNAL(timeout()), this, SLOT(repeatKey()));
}

QWaylandInputDevice::~QWaylandInputDevice()
{
    delete mPointer;
    delete mKeyboard;
    delete mTouch;
}

void QWaylandInputDevice::seat_capabilities(uint32_t caps)
{
    mCaps = caps;

    if (caps & WL_SEAT_CAPABILITY_KEYBOARD && !mKeyboard) {
        mKeyboard = new Keyboard(this);
        mKeyboard->init(get_keyboard());
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && mKeyboard) {
        delete mKeyboard;
        mKeyboard = 0;
        mRepeatTimer.stop();
    }

    if (caps & WL_SEAT_CAPABILITY_POINTER && !mPointer) {
        mPointer = new Pointer(this);
        mPointer->init(get_pointer());
        pointerSurface = mQDisplay->createSurface(this);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && mPointer) {
        delete mPointer;
        mPointer = 0;
    }

    if (caps & WL_SEAT_CAPABILITY_TOUCH && !mTouch) {
        mTouch = new Touch(this);
        mTouch->init(get_touch());

        if (!mTouchDevice) {
            mTouchDevice = new QTouchDevice;
            mTouchDevice->setType(QTouchDevice::TouchScreen);
            mTouchDevice->setCapabilities(QTouchDevice::Position);
            QWindowSystemInterface::registerTouchDevice(mTouchDevice);
        }
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && mTouch) {
        delete mTouch;
        mTouch = 0;
    }
}

void QWaylandInputDevice::handleWindowDestroyed(QWaylandWindow *window)
{
    if (mPointer && window == mPointer->mFocus)
        mPointer->mFocus = 0;
    if (mKeyboard && window == mKeyboard->mFocus) {
        mKeyboard->mFocus = 0;
        mRepeatTimer.stop();
    }
}

void QWaylandInputDevice::setDataDevice(QWaylandDataDevice *device)
{
    mDataDevice = device;
}

QWaylandDataDevice *QWaylandInputDevice::dataDevice() const
{
    Q_ASSERT(mDataDevice);
    return mDataDevice;
}

void QWaylandInputDevice::removeMouseButtonFromState(Qt::MouseButton button)
{
    if (mPointer)
        mPointer->mButtons = mPointer->mButtons & !button;
}

QWaylandWindow *QWaylandInputDevice::pointerFocus() const
{
    return mPointer ? mPointer->mFocus : 0;
}

QWaylandWindow *QWaylandInputDevice::keyboardFocus() const
{
    return mKeyboard ? mKeyboard->mFocus : 0;
}

QWaylandWindow *QWaylandInputDevice::touchFocus() const
{
    return mTouch ? mTouch->mFocus : 0;
}

Qt::KeyboardModifiers QWaylandInputDevice::modifiers() const
{
    if (!mKeyboard)
        return Qt::NoModifier;

    Qt::KeyboardModifiers ret = Qt::NoModifier;

#ifndef QT_NO_WAYLAND_XKB
    xkb_state_component cstate = static_cast<xkb_state_component>(XKB_STATE_DEPRESSED | XKB_STATE_LATCHED);

    if (xkb_state_mod_name_is_active(mKeyboard->mXkbState, "Shift", cstate))
        ret |= Qt::ShiftModifier;
    if (xkb_state_mod_name_is_active(mKeyboard->mXkbState, "Control", cstate))
        ret |= Qt::ControlModifier;
    if (xkb_state_mod_name_is_active(mKeyboard->mXkbState, "Alt", cstate))
        ret |= Qt::AltModifier;
    if (xkb_state_mod_name_is_active(mKeyboard->mXkbState, "Mod1", cstate))
        ret |= Qt::AltModifier;
    if (xkb_state_mod_name_is_active(mKeyboard->mXkbState, "Mod4", cstate))
        ret |= Qt::MetaModifier;
#endif

    return ret;
}

uint32_t QWaylandInputDevice::cursorSerial() const
{
    if (mPointer)
        return mPointer->mCursorSerial;
    return 0;
}

void QWaylandInputDevice::setCursor(Qt::CursorShape newShape, QWaylandScreen *screen)
{
    struct wl_cursor_image *image = screen->waylandCursor()->cursorImage(newShape);
    if (!image) {
        return;
    }

    struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
    setCursor(buffer, image);
}

void QWaylandInputDevice::setCursor(struct wl_buffer *buffer, struct wl_cursor_image *image)
{
    if (mCaps & WL_SEAT_CAPABILITY_POINTER) {
        mPointer->mCursorSerial = mPointer->mEnterSerial;
        /* Hide cursor */
        if (!buffer)
        {
            mPointer->set_cursor(mPointer->mEnterSerial, NULL, 0, 0);
            return;
        }

        mPointer->set_cursor(mPointer->mEnterSerial, pointerSurface,
                             image->hotspot_x, image->hotspot_y);
        wl_surface_attach(pointerSurface, buffer, 0, 0);
        wl_surface_damage(pointerSurface, 0, 0, image->width, image->height);
        wl_surface_commit(pointerSurface);
    }
}

void QWaylandInputDevice::Pointer::pointer_enter(uint32_t serial, struct wl_surface *surface,
                                                 wl_fixed_t sx, wl_fixed_t sy)
{
    Q_UNUSED(sx);
    Q_UNUSED(sy);

    if (!surface)
        return;

    QWaylandWindow *window = QWaylandWindow::fromWlSurface(surface);
    window->window()->setCursor(window->window()->cursor());

    mFocus = window;

    mParent->mTime = QWaylandDisplay::currentTimeMillisec();
    mParent->mSerial = serial;
    mEnterSerial = serial;

    QWaylandWindow *grab = QWaylandWindow::mouseGrab();
    if (!grab) {
        window->handleMouseEnter(mParent);
        window->handleMouse(mParent, mParent->mTime, mSurfacePos, mGlobalPos, mButtons, Qt::NoModifier);
    }
}

void QWaylandInputDevice::Pointer::pointer_leave(uint32_t time, struct wl_surface *surface)
{
    // The event may arrive after destroying the window, indicated by
    // a null surface.
    if (!surface)
        return;

    if (!QWaylandWindow::mouseGrab()) {
        QWaylandWindow *window = QWaylandWindow::fromWlSurface(surface);
        window->handleMouseLeave(mParent);
    }
    mFocus = 0;
    mButtons = Qt::NoButton;

    mParent->mTime = time;
}

void QWaylandInputDevice::Pointer::pointer_motion(uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    Q_UNUSED(surface_x);
    Q_UNUSED(surface_y);

    QWaylandWindow *window = mFocus;

    if (window == NULL) {
        // We destroyed the pointer focus surface, but the server
        // didn't get the message yet.
        return;
    }

    QPointF pos(wl_fixed_to_double(surface_x), wl_fixed_to_double(surface_y));
    QPointF delta = pos - pos.toPoint();
    QPointF global = window->window()->mapToGlobal(pos.toPoint());
    global += delta;

    mSurfacePos = pos;
    mGlobalPos = global;
    mParent->mTime = time;

    QWaylandWindow *grab = QWaylandWindow::mouseGrab();
    if (grab && grab != window) {
        // We can't know the true position since we're getting events for another surface,
        // so we just set it outside of the window boundaries.
        pos = QPointF(-1, -1);
        global = grab->window()->mapToGlobal(pos.toPoint());
        grab->handleMouse(mParent, time, pos, global, mButtons, Qt::NoModifier);
    } else
        window->handleMouse(mParent, time, mSurfacePos, mGlobalPos, mButtons, Qt::NoModifier);
}

void QWaylandInputDevice::Pointer::pointer_button(uint32_t serial, uint32_t time,
                                                  uint32_t button, uint32_t state)
{
    Q_UNUSED(serial);
    QWaylandWindow *window = mFocus;
    Qt::MouseButton qt_button;

    // translate from kernel (input.h) 'button' to corresponding Qt:MouseButton.
    // The range of mouse values is 0x110 <= mouse_button < 0x120, the first Joystick button.
    switch (button) {
    case 0x110: qt_button = Qt::LeftButton; break;    // kernel BTN_LEFT
    case 0x111: qt_button = Qt::RightButton; break;
    case 0x112: qt_button = Qt::MiddleButton; break;
    case 0x113: qt_button = Qt::ExtraButton1; break;  // AKA Qt::BackButton
    case 0x114: qt_button = Qt::ExtraButton2; break;  // AKA Qt::ForwardButton
    case 0x115: qt_button = Qt::ExtraButton3; break;  // AKA Qt::TaskButton
    case 0x116: qt_button = Qt::ExtraButton4; break;
    case 0x117: qt_button = Qt::ExtraButton5; break;
    case 0x118: qt_button = Qt::ExtraButton6; break;
    case 0x119: qt_button = Qt::ExtraButton7; break;
    case 0x11a: qt_button = Qt::ExtraButton8; break;
    case 0x11b: qt_button = Qt::ExtraButton9; break;
    case 0x11c: qt_button = Qt::ExtraButton10; break;
    case 0x11d: qt_button = Qt::ExtraButton11; break;
    case 0x11e: qt_button = Qt::ExtraButton12; break;
    case 0x11f: qt_button = Qt::ExtraButton13; break;
    default: return; // invalid button number (as far as Qt is concerned)
    }

    if (state)
        mButtons |= qt_button;
    else
        mButtons &= ~qt_button;

    mParent->mTime = time;
    mParent->mSerial = serial;

    QWaylandWindow *grab = QWaylandWindow::mouseGrab();
    if (grab && grab != mFocus) {
        QPointF pos = QPointF(-1, -1);
        QPointF global = grab->window()->mapToGlobal(pos.toPoint());
        grab->handleMouse(mParent, time, pos, global, mButtons, Qt::NoModifier);
    } else if (window)
        window->handleMouse(mParent, time, mSurfacePos, mGlobalPos, mButtons, Qt::NoModifier);
}

void QWaylandInputDevice::Pointer::pointer_axis(uint32_t time, uint32_t axis, int32_t value)
{
    QWaylandWindow *window = mFocus;
    QPoint pixelDelta;
    QPoint angleDelta;

    if (window == NULL) {
        // We destroyed the pointer focus surface, but the server
        // didn't get the message yet.
        return;
    }

    //normalize value and inverse axis
    int valueDelta = wl_fixed_to_int(value) * -12;

    if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
        pixelDelta = QPoint();
        angleDelta.setX(valueDelta);
    } else {
        pixelDelta = QPoint();
        angleDelta.setY(valueDelta);
    }

    QWindowSystemInterface::handleWheelEvent(window->window(),
                                             time, mSurfacePos,
                                             mGlobalPos, pixelDelta,
                                             angleDelta);
}

#ifndef QT_NO_WAYLAND_XKB

static const uint32_t KeyTbl[] = {
    XKB_KEY_Escape,                  Qt::Key_Escape,
    XKB_KEY_Tab,                     Qt::Key_Tab,
    XKB_KEY_ISO_Left_Tab,            Qt::Key_Backtab,
    XKB_KEY_BackSpace,               Qt::Key_Backspace,
    XKB_KEY_Return,                  Qt::Key_Return,
    XKB_KEY_Insert,                  Qt::Key_Insert,
    XKB_KEY_Delete,                  Qt::Key_Delete,
    XKB_KEY_Clear,                   Qt::Key_Delete,
    XKB_KEY_Pause,                   Qt::Key_Pause,
    XKB_KEY_Print,                   Qt::Key_Print,

    XKB_KEY_Home,                    Qt::Key_Home,
    XKB_KEY_End,                     Qt::Key_End,
    XKB_KEY_Left,                    Qt::Key_Left,
    XKB_KEY_Up,                      Qt::Key_Up,
    XKB_KEY_Right,                   Qt::Key_Right,
    XKB_KEY_Down,                    Qt::Key_Down,
    XKB_KEY_Prior,                   Qt::Key_PageUp,
    XKB_KEY_Next,                    Qt::Key_PageDown,

    XKB_KEY_Shift_L,                 Qt::Key_Shift,
    XKB_KEY_Shift_R,                 Qt::Key_Shift,
    XKB_KEY_Shift_Lock,              Qt::Key_Shift,
    XKB_KEY_Control_L,               Qt::Key_Control,
    XKB_KEY_Control_R,               Qt::Key_Control,
    XKB_KEY_Meta_L,                  Qt::Key_Meta,
    XKB_KEY_Meta_R,                  Qt::Key_Meta,
    XKB_KEY_Alt_L,                   Qt::Key_Alt,
    XKB_KEY_Alt_R,                   Qt::Key_Alt,
    XKB_KEY_Caps_Lock,               Qt::Key_CapsLock,
    XKB_KEY_Num_Lock,                Qt::Key_NumLock,
    XKB_KEY_Scroll_Lock,             Qt::Key_ScrollLock,
    XKB_KEY_Super_L,                 Qt::Key_Super_L,
    XKB_KEY_Super_R,                 Qt::Key_Super_R,
    XKB_KEY_Menu,                    Qt::Key_Menu,
    XKB_KEY_Hyper_L,                 Qt::Key_Hyper_L,
    XKB_KEY_Hyper_R,                 Qt::Key_Hyper_R,
    XKB_KEY_Help,                    Qt::Key_Help,

    XKB_KEY_KP_Space,                Qt::Key_Space,
    XKB_KEY_KP_Tab,                  Qt::Key_Tab,
    XKB_KEY_KP_Enter,                Qt::Key_Enter,
    XKB_KEY_KP_Home,                 Qt::Key_Home,
    XKB_KEY_KP_Left,                 Qt::Key_Left,
    XKB_KEY_KP_Up,                   Qt::Key_Up,
    XKB_KEY_KP_Right,                Qt::Key_Right,
    XKB_KEY_KP_Down,                 Qt::Key_Down,
    XKB_KEY_KP_Prior,                Qt::Key_PageUp,
    XKB_KEY_KP_Next,                 Qt::Key_PageDown,
    XKB_KEY_KP_End,                  Qt::Key_End,
    XKB_KEY_KP_Begin,                Qt::Key_Clear,
    XKB_KEY_KP_Insert,               Qt::Key_Insert,
    XKB_KEY_KP_Delete,               Qt::Key_Delete,
    XKB_KEY_KP_Equal,                Qt::Key_Equal,
    XKB_KEY_KP_Multiply,             Qt::Key_Asterisk,
    XKB_KEY_KP_Add,                  Qt::Key_Plus,
    XKB_KEY_KP_Separator,            Qt::Key_Comma,
    XKB_KEY_KP_Subtract,             Qt::Key_Minus,
    XKB_KEY_KP_Decimal,              Qt::Key_Period,
    XKB_KEY_KP_Divide,               Qt::Key_Slash,

    XKB_KEY_ISO_Level3_Shift,        Qt::Key_AltGr,
    XKB_KEY_Multi_key,               Qt::Key_Multi_key,
    XKB_KEY_Codeinput,               Qt::Key_Codeinput,
    XKB_KEY_SingleCandidate,         Qt::Key_SingleCandidate,
    XKB_KEY_MultipleCandidate,       Qt::Key_MultipleCandidate,
    XKB_KEY_PreviousCandidate,       Qt::Key_PreviousCandidate,

    XKB_KEY_Mode_switch,             Qt::Key_Mode_switch,
    XKB_KEY_script_switch,           Qt::Key_Mode_switch,

    0,                          0
};

static int keysymToQtKey(xkb_keysym_t key)
{
    int code = 0;
    int i = 0;
    while (KeyTbl[i]) {
        if (key == KeyTbl[i]) {
            code = (int)KeyTbl[i+1];
            break;
        }
        i += 2;
    }

    return code;
}

static int keysymToQtKey(xkb_keysym_t keysym, Qt::KeyboardModifiers &modifiers, const QString &text)
{
    int code = 0;

    if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F35) {
        code =  Qt::Key_F1 + (int(keysym) - XKB_KEY_F1);
    } else if (keysym >= XKB_KEY_KP_Space && keysym <= XKB_KEY_KP_9) {
        if (keysym >= XKB_KEY_KP_0) {
            // numeric keypad keys
            code = Qt::Key_0 + ((int)keysym - XKB_KEY_KP_0);
        } else {
            code = keysymToQtKey(keysym);
        }
        modifiers |= Qt::KeypadModifier;
    } else if (text.length() == 1 && text.unicode()->unicode() > 0x1f
        && text.unicode()->unicode() != 0x7f
        && !(keysym >= XKB_KEY_dead_grave && keysym <= XKB_KEY_dead_currency)) {
        code = text.unicode()->toUpper().unicode();
    } else {
        // any other keys
        code = keysymToQtKey(keysym);
    }

    return code;
}

#endif // QT_NO_WAYLAND_XKB

void QWaylandInputDevice::Keyboard::keyboard_keymap(uint32_t format, int32_t fd, uint32_t size)
{
#ifndef QT_NO_WAYLAND_XKB
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *map_str = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    mXkbMap = xkb_map_new_from_string(mXkbContext, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, (xkb_keymap_compile_flags)0);
    munmap(map_str, size);
    close(fd);

    mXkbState = xkb_state_new(mXkbMap);
#else
    Q_UNUSED(format);
    Q_UNUSED(fd);
    Q_UNUSED(size);
#endif
}

void QWaylandInputDevice::Keyboard::keyboard_enter(uint32_t time, struct wl_surface *surface, struct wl_array *keys)
{
    Q_UNUSED(time);
    Q_UNUSED(keys);

    if (!surface)
        return;


    QWaylandWindow *window = QWaylandWindow::fromWlSurface(surface);
    mFocus = window;

    if (!mFocusCallback) {
        mFocusCallback = wl_display_sync(mParent->mDisplay);
        wl_callback_add_listener(mFocusCallback, &QWaylandInputDevice::Keyboard::callback, this);
    }
}

void QWaylandInputDevice::Keyboard::keyboard_leave(uint32_t time, struct wl_surface *surface)
{
    Q_UNUSED(time);
    Q_UNUSED(surface);

    if (surface) {
        QWaylandWindow *window = QWaylandWindow::fromWlSurface(surface);
        window->unfocus();
    }

    mFocus = NULL;

    // Use a callback to set the focus because we may get a leave/enter pair, and
    // the latter one would be lost in the QWindowSystemInterface queue, if
    // we issue the handleWindowActivated() calls immediately.
    if (!mFocusCallback) {
        mFocusCallback = wl_display_sync(mParent->mDisplay);
        wl_callback_add_listener(mFocusCallback, &QWaylandInputDevice::Keyboard::callback, this);
    }
    mParent->mRepeatTimer.stop();
}

const wl_callback_listener QWaylandInputDevice::Keyboard::callback = {
    QWaylandInputDevice::Keyboard::focusCallback
};

void QWaylandInputDevice::Keyboard::focusCallback(void *data, struct wl_callback *callback, uint32_t time)
{
    Q_UNUSED(time);
    Q_UNUSED(callback);
    QWaylandInputDevice::Keyboard *self = static_cast<QWaylandInputDevice::Keyboard *>(data);
    if (self->mFocusCallback) {
        wl_callback_destroy(self->mFocusCallback);
        self->mFocusCallback = 0;
    }

    self->mParent->mQDisplay->setLastKeyboardFocusInputDevice(self->mFocus ? self->mParent : 0);
    QWindowSystemInterface::handleWindowActivated(self->mFocus ? self->mFocus->window() : 0);
}

void QWaylandInputDevice::Keyboard::keyboard_key(uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    Q_UNUSED(serial);
    QWaylandWindow *window = mFocus;
    uint32_t code = key + 8;
    bool isDown = state != 0;
    QEvent::Type type = isDown ? QEvent::KeyPress : QEvent::KeyRelease;
    QString text;
    int qtkey = key + 8;  // qt-compositor substracts 8 for some reason

    if (!window) {
        // We destroyed the keyboard focus surface, but the server
        // didn't get the message yet.
        return;
    }

#ifndef QT_NO_WAYLAND_XKB
    if (!mXkbMap)
        return;

    const xkb_keysym_t sym = xkb_state_key_get_one_sym(mXkbState, code);
    xkb_state_update_key(mXkbState, code, isDown ? XKB_KEY_DOWN : XKB_KEY_UP);

    Qt::KeyboardModifiers modifiers = mParent->modifiers();

    uint utf32 = xkb_keysym_to_utf32(sym);
    text = QString::fromUcs4(&utf32, 1);

    qtkey = keysymToQtKey(sym, modifiers, text);

    QWindowSystemInterface::handleExtendedKeyEvent(window->window(),
                                                    time, type, qtkey,
                                                    modifiers,
                                                    code, sym, mNativeModifiers, text);
#else
    // Generic fallback for single hard keys: Assume 'key' is a Qt key code.
    QWindowSystemInterface::handleExtendedKeyEvent(window->window(),
                                                    time, type,
                                                    qtkey,
                                                    Qt::NoModifier,
                                                    code, 0, 0);
#endif

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED
#ifndef QT_NO_WAYLAND_XKB
        && xkb_keymap_key_repeats(mXkbMap, code)
#endif
        ) {
        mRepeatKey = qtkey;
        mRepeatCode = code;
        mRepeatTime = time;
        mRepeatText = text;
#ifndef QT_NO_WAYLAND_XKB
        mRepeatSym = sym;
#endif
        mParent->mRepeatTimer.setInterval(400);
        mParent->mRepeatTimer.start();
    } else if (mRepeatCode == code) {
        mParent->mRepeatTimer.stop();
    }
}

void QWaylandInputDevice::repeatKey()
{
    mRepeatTimer.setInterval(25);

    QWindowSystemInterface::handleExtendedKeyEvent(mKeyboard->mFocus->window(),
                                                   mKeyboard->mRepeatTime, QEvent::KeyRelease, mKeyboard->mRepeatKey,
                                                   modifiers(),
                                                   mKeyboard->mRepeatCode,
#ifndef QT_NO_WAYLAND_XKB
                                                   mKeyboard->mRepeatSym, mKeyboard->mNativeModifiers,
#else
                                                   0, 0,
#endif
                                                   mKeyboard->mRepeatText, true);

    QWindowSystemInterface::handleExtendedKeyEvent(mKeyboard->mFocus->window(),
                                                   mKeyboard->mRepeatTime, QEvent::KeyPress, mKeyboard->mRepeatKey,
                                                   modifiers(),
                                                   mKeyboard->mRepeatCode,
#ifndef QT_NO_WAYLAND_XKB
                                                   mKeyboard->mRepeatSym, mKeyboard->mNativeModifiers,
#else
                                                   0, 0,
#endif
                                                   mKeyboard->mRepeatText, true);
}

void QWaylandInputDevice::Keyboard::keyboard_modifiers(uint32_t serial,
                                             uint32_t mods_depressed,
                                             uint32_t mods_latched,
                                             uint32_t mods_locked,
                                             uint32_t group)
{
    Q_UNUSED(serial);
#ifndef QT_NO_WAYLAND_XKB
    if (mXkbState)
        xkb_state_update_mask(mXkbState,
                              mods_depressed, mods_latched, mods_locked,
                              0, 0, group);
    mNativeModifiers = mods_depressed | mods_latched | mods_locked;
#else
    Q_UNUSED(serial);
    Q_UNUSED(mods_depressed);
    Q_UNUSED(mods_latched);
    Q_UNUSED(mods_locked);
    Q_UNUSED(group);
#endif
}

void QWaylandInputDevice::Touch::touch_down(uint32_t serial,
                                     uint32_t time,
                                     struct wl_surface *surface,
                                     int32_t id,
                                     wl_fixed_t x,
                                     wl_fixed_t y)
{
    mParent->mTime = time;
    mParent->mSerial = serial;
    mFocus = QWaylandWindow::fromWlSurface(surface);
    mParent->handleTouchPoint(id, wl_fixed_to_double(x), wl_fixed_to_double(y), Qt::TouchPointPressed);
}

void QWaylandInputDevice::Touch::touch_up(uint32_t serial, uint32_t time, int32_t id)
{
    Q_UNUSED(serial);
    Q_UNUSED(time);
    mFocus = 0;
    mParent->handleTouchPoint(id, 0, 0, Qt::TouchPointReleased);

    // As of Weston 1.5.90 there is no touch_frame after the last touch_up
    // (i.e. when the last finger is released). To accommodate for this, issue a
    // touch_frame. This cannot hurt since it is safe to call the touch_frame
    // handler multiple times when there are no points left.
    if (allTouchPointsReleased())
        touch_frame();
}

void QWaylandInputDevice::Touch::touch_motion(uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    Q_UNUSED(time);
    mParent->handleTouchPoint(id, wl_fixed_to_double(x), wl_fixed_to_double(y), Qt::TouchPointMoved);
}

void QWaylandInputDevice::Touch::touch_cancel()
{
    mPrevTouchPoints.clear();
    mTouchPoints.clear();

    QWaylandTouchExtension *touchExt = mParent->mQDisplay->touchExtension();
    if (touchExt)
        touchExt->touchCanceled();

    QWindowSystemInterface::handleTouchCancelEvent(0, mParent->mTouchDevice);
}

void QWaylandInputDevice::handleTouchPoint(int id, double x, double y, Qt::TouchPointState state)
{
    QWindowSystemInterface::TouchPoint tp;

    // Find out the coordinates for Released events.
    bool coordsOk = false;
    if (state == Qt::TouchPointReleased)
        for (int i = 0; i < mTouch->mPrevTouchPoints.count(); ++i)
            if (mTouch->mPrevTouchPoints.at(i).id == id) {
                tp.area = mTouch->mPrevTouchPoints.at(i).area;
                coordsOk = true;
                break;
            }

    if (!coordsOk) {
        // x and y are surface relative.
        // We need a global (screen) position.
        QWaylandWindow *win = mTouch->mFocus;

        //is it possible that mTouchFocus is null;
        if (!win && mPointer)
            win = mPointer->mFocus;
        if (!win && mKeyboard)
            win = mKeyboard->mFocus;
        if (!win || !win->window())
            return;

        tp.area = QRectF(0, 0, 8, 8);
        QMargins margins = win->frameMargins();
        tp.area.moveCenter(win->window()->mapToGlobal(QPoint(x - margins.left(), y - margins.top())));
    }

    tp.state = state;
    tp.id = id;
    tp.pressure = tp.state == Qt::TouchPointReleased ? 0 : 1;
    mTouch->mTouchPoints.append(tp);
}

bool QWaylandInputDevice::Touch::allTouchPointsReleased()
{
    for (int i = 0; i < mTouchPoints.count(); ++i)
        if (mTouchPoints.at(i).state != Qt::TouchPointReleased)
            return false;

    return true;
}

void QWaylandInputDevice::Touch::touch_frame()
{
    // Copy all points, that are in the previous but not in the current list, as stationary.
    for (int i = 0; i < mPrevTouchPoints.count(); ++i) {
        const QWindowSystemInterface::TouchPoint &prevPoint(mPrevTouchPoints.at(i));
        if (prevPoint.state == Qt::TouchPointReleased)
            continue;
        bool found = false;
        for (int j = 0; j < mTouchPoints.count(); ++j)
            if (mTouchPoints.at(j).id == prevPoint.id) {
                found = true;
                break;
            }
        if (!found) {
            QWindowSystemInterface::TouchPoint p = prevPoint;
            p.state = Qt::TouchPointStationary;
            mTouchPoints.append(p);
        }
    }

    if (mTouchPoints.isEmpty()) {
        mPrevTouchPoints.clear();
        return;
    }

    QWindow *window = mFocus ? mFocus->window() : 0;

    if (mFocus) {
        const QWindowSystemInterface::TouchPoint &tp = mTouchPoints.last();
        // When the touch event is received, the global pos is calculated with the margins
        // in mind. Now we need to adjust again to get the correct local pos back.
        QMargins margins = window->frameMargins();
        QPoint p = tp.area.center().toPoint();
        QPointF localPos(window->mapFromGlobal(QPoint(p.x() + margins.left(), p.y() + margins.top())));
        if (mFocus->touchDragDecoration(mParent, localPos, tp.area.center(), tp.state, mParent->modifiers()))
            return;
    }
    QWindowSystemInterface::handleTouchEvent(window, mParent->mTouchDevice, mTouchPoints);

    if (allTouchPointsReleased())
        mPrevTouchPoints.clear();
    else
        mPrevTouchPoints = mTouchPoints;

    mTouchPoints.clear();
}

QT_END_NAMESPACE
