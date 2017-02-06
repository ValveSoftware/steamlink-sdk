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
#include "qwaylandshmbackingstore_p.h"
#include "../shared/qwaylandxkb_p.h"
#include "qwaylandinputcontext_p.h"

#include <QtGui/private/qpixmap_raster_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qplatforminputcontext.h>
#include <QDebug>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <wayland-cursor.h>

#include <QtGui/QGuiApplication>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandInputDevice::Keyboard::Keyboard(QWaylandInputDevice *p)
    : mParent(p)
    , mFocus(0)
#if QT_CONFIG(xkbcommon_evdev)
    , mXkbContext(0)
    , mXkbMap(0)
    , mXkbState(0)
#endif
    , mNativeModifiers(0)
{
    connect(&mRepeatTimer, SIGNAL(timeout()), this, SLOT(repeatKey()));
}

#if QT_CONFIG(xkbcommon_evdev)
bool QWaylandInputDevice::Keyboard::createDefaultKeyMap()
{
    if (mXkbContext && mXkbMap && mXkbState) {
        return true;
    }

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

    if (!mXkbContext || !mXkbMap || !mXkbState) {
        qWarning() << "xkb_map_new_from_names failed, no key input";
        return false;
    }
    return true;
}

void QWaylandInputDevice::Keyboard::releaseKeyMap()
{
    if (mXkbState)
        xkb_state_unref(mXkbState);
    if (mXkbMap)
        xkb_map_unref(mXkbMap);
    if (mXkbContext)
        xkb_context_unref(mXkbContext);
}
#endif

QWaylandInputDevice::Keyboard::~Keyboard()
{
#if QT_CONFIG(xkbcommon_evdev)
    releaseKeyMap();
#endif
    if (mFocus)
        QWindowSystemInterface::handleWindowActivated(0);
    if (mParent->mVersion >= 3)
        wl_keyboard_release(object());
    else
        wl_keyboard_destroy(object());
}

void QWaylandInputDevice::Keyboard::stopRepeat()
{
    mRepeatTimer.stop();
}

QWaylandInputDevice::Pointer::Pointer(QWaylandInputDevice *p)
    : mParent(p)
    , mFocus(0)
    , mEnterSerial(0)
    , mCursorSerial(0)
    , mButtons(0)
    , mCursorBuffer(nullptr)
    , mCursorShape(Qt::BitmapCursor)
{
}

QWaylandInputDevice::Pointer::~Pointer()
{
    if (mParent->mVersion >= 3)
        wl_pointer_release(object());
    else
        wl_pointer_destroy(object());
}

QWaylandInputDevice::Touch::Touch(QWaylandInputDevice *p)
    : mParent(p)
    , mFocus(0)
{
}

QWaylandInputDevice::Touch::~Touch()
{
    if (mParent->mVersion >= 3)
        wl_touch_release(object());
    else
        wl_touch_destroy(object());
}

QWaylandInputDevice::QWaylandInputDevice(QWaylandDisplay *display, int version, uint32_t id)
    : QObject()
    , QtWayland::wl_seat(display->wl_registry(), id, qMin(version, 4))
    , mQDisplay(display)
    , mDisplay(display->wl_display())
    , mVersion(qMin(version, 4))
    , mCaps(0)
    , mDataDevice(0)
    , mKeyboard(0)
    , mPointer(0)
    , mTouch(0)
    , mTextInput(0)
    , mTime(0)
    , mSerial(0)
    , mTouchDevice(0)
{
#if QT_CONFIG(draganddrop)
    if (mQDisplay->dndSelectionHandler()) {
        mDataDevice = mQDisplay->dndSelectionHandler()->getDataDevice(this);
    }
#endif

    if (mQDisplay->textInputManager()) {
        mTextInput = new QWaylandTextInput(mQDisplay, mQDisplay->textInputManager()->get_text_input(wl_seat()));
    }
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
        mKeyboard = createKeyboard(this);
        mKeyboard->init(get_keyboard());
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && mKeyboard) {
        delete mKeyboard;
        mKeyboard = 0;
    }

    if (caps & WL_SEAT_CAPABILITY_POINTER && !mPointer) {
        mPointer = createPointer(this);
        mPointer->init(get_pointer());
        pointerSurface = mQDisplay->createSurface(this);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && mPointer) {
        delete mPointer;
        mPointer = 0;
    }

    if (caps & WL_SEAT_CAPABILITY_TOUCH && !mTouch) {
        mTouch = createTouch(this);
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

QWaylandInputDevice::Keyboard *QWaylandInputDevice::createKeyboard(QWaylandInputDevice *device)
{
    return new Keyboard(device);
}

QWaylandInputDevice::Pointer *QWaylandInputDevice::createPointer(QWaylandInputDevice *device)
{
    return new Pointer(device);
}

QWaylandInputDevice::Touch *QWaylandInputDevice::createTouch(QWaylandInputDevice *device)
{
    return new Touch(device);
}

void QWaylandInputDevice::handleWindowDestroyed(QWaylandWindow *window)
{
    if (mPointer && window == mPointer->mFocus)
        mPointer->mFocus = 0;
    if (mKeyboard && window == mKeyboard->mFocus) {
        mKeyboard->mFocus = 0;
        mKeyboard->stopRepeat();
    }
    if (mTouch && window == mTouch->mFocus)
        mTouch->mFocus = 0;
}

void QWaylandInputDevice::handleEndDrag()
{
    if (mTouch)
        mTouch->releasePoints();
    if (mPointer)
        mPointer->releaseButtons();
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

void QWaylandInputDevice::setTextInput(QWaylandTextInput *textInput)
{
    mTextInput = textInput;
}

QWaylandTextInput *QWaylandInputDevice::textInput() const
{
    return mTextInput;
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

    return mKeyboard->modifiers();
}

Qt::KeyboardModifiers QWaylandInputDevice::Keyboard::modifiers() const
{
    Qt::KeyboardModifiers ret = Qt::NoModifier;

#if QT_CONFIG(xkbcommon_evdev)
    if (!mXkbState)
        return ret;

    ret = QWaylandXkb::modifiers(mXkbState);
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

void QWaylandInputDevice::setCursor(const QCursor &cursor, QWaylandScreen *screen)
{
    if (cursor.shape() != Qt::BitmapCursor && cursor.shape() == mPointer->mCursorShape)
        return;

    mPointer->mCursorShape = cursor.shape();
    if (cursor.shape() == Qt::BitmapCursor) {
        setCursor(screen->waylandCursor()->cursorBitmapImage(&cursor), cursor.hotSpot());
        return;
    }
    setCursor(cursor.shape(), screen);
}

void QWaylandInputDevice::setCursor(struct wl_buffer *buffer, struct wl_cursor_image *image)
{
    setCursor(buffer,
              image ? QPoint(image->hotspot_x, image->hotspot_y) : QPoint(),
              image ? QSize(image->width, image->height) : QSize());
}

void QWaylandInputDevice::setCursor(struct wl_buffer *buffer, const QPoint &hotSpot, const QSize &size)
{
    if (mCaps & WL_SEAT_CAPABILITY_POINTER) {
        bool force = mPointer->mEnterSerial > mPointer->mCursorSerial;

        if (!force && mPointer->mCursorBuffer == buffer)
            return;

        mPixmapCursor.clear();
        mPointer->mCursorSerial = mPointer->mEnterSerial;

        mPointer->mCursorBuffer = buffer;

        /* Hide cursor */
        if (!buffer)
        {
            mPointer->set_cursor(mPointer->mEnterSerial, NULL, 0, 0);
            return;
        }

        mPointer->set_cursor(mPointer->mEnterSerial, pointerSurface,
                             hotSpot.x(), hotSpot.y());
        wl_surface_attach(pointerSurface, buffer, 0, 0);
        wl_surface_damage(pointerSurface, 0, 0, size.width(), size.height());
        wl_surface_commit(pointerSurface);
    }
}

void QWaylandInputDevice::setCursor(const QSharedPointer<QWaylandBuffer> &buffer, const QPoint &hotSpot)
{
    setCursor(buffer->buffer(), hotSpot, buffer->size());
    mPixmapCursor = buffer;
}

class EnterEvent : public QWaylandPointerEvent
{
public:
    EnterEvent(const QPointF &l, const QPointF &g)
        : QWaylandPointerEvent(QWaylandPointerEvent::Enter, 0, l, g, 0, Qt::NoModifier)
    {}
};

void QWaylandInputDevice::Pointer::pointer_enter(uint32_t serial, struct wl_surface *surface,
                                                 wl_fixed_t sx, wl_fixed_t sy)
{
    if (!surface)
        return;

    QWaylandWindow *window = QWaylandWindow::fromWlSurface(surface);
    window->window()->setCursor(window->window()->cursor());

    mFocus = window;
    mSurfacePos = QPointF(wl_fixed_to_double(sx), wl_fixed_to_double(sy));
    mGlobalPos = window->window()->mapToGlobal(mSurfacePos.toPoint());

    mParent->mSerial = serial;
    mEnterSerial = serial;

    QWaylandWindow *grab = QWaylandWindow::mouseGrab();
    if (!grab) {
        EnterEvent evt(mSurfacePos, mGlobalPos);
        window->handleMouse(mParent, evt);
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

class MotionEvent : public QWaylandPointerEvent
{
public:
    MotionEvent(ulong t, const QPointF &l, const QPointF &g, Qt::MouseButtons b, Qt::KeyboardModifiers m)
        : QWaylandPointerEvent(QWaylandPointerEvent::Motion, t, l, g, b, m)
    {
    }
};

void QWaylandInputDevice::Pointer::pointer_motion(uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
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
        MotionEvent e(time, pos, global, mButtons, mParent->modifiers());
        grab->handleMouse(mParent, e);
    } else {
        MotionEvent e(time, mSurfacePos, mGlobalPos, mButtons, mParent->modifiers());
        window->handleMouse(mParent, e);
    }
}

void QWaylandInputDevice::Pointer::pointer_button(uint32_t serial, uint32_t time,
                                                  uint32_t button, uint32_t state)
{
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
    if (state)
        mParent->mQDisplay->setLastInputDevice(mParent, serial, window);

    QWaylandWindow *grab = QWaylandWindow::mouseGrab();
    if (grab && grab != mFocus) {
        QPointF pos = QPointF(-1, -1);
        QPointF global = grab->window()->mapToGlobal(pos.toPoint());
        MotionEvent e(time, pos, global, mButtons, mParent->modifiers());
        grab->handleMouse(mParent, e);
    } else if (window) {
        MotionEvent e(time, mSurfacePos, mGlobalPos, mButtons, mParent->modifiers());
        window->handleMouse(mParent, e);
    }
}

void QWaylandInputDevice::Pointer::releaseButtons()
{
    mButtons = Qt::NoButton;
    MotionEvent e(mParent->mTime, mSurfacePos, mGlobalPos, mButtons, mParent->modifiers());
    if (mFocus)
        mFocus->handleMouse(mParent, e);
}

class WheelEvent : public QWaylandPointerEvent
{
public:
    WheelEvent(ulong t, const QPointF &l, const QPointF &g, const QPoint &pd, const QPoint &ad)
        : QWaylandPointerEvent(QWaylandPointerEvent::Wheel, t, l, g, pd, ad)
    {
    }
};

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

    WheelEvent e(time, mSurfacePos, mGlobalPos, pixelDelta, angleDelta);
    window->handleMouse(mParent, e);
}

void QWaylandInputDevice::Keyboard::keyboard_keymap(uint32_t format, int32_t fd, uint32_t size)
{
#if QT_CONFIG(xkbcommon_evdev)
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *map_str = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    // Release the old keymap resources in the case they were already created in
    // the key event or when the compositor issues a new map
    releaseKeyMap();

    mXkbContext = xkb_context_new(xkb_context_flags(0));
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

    mParent->mQDisplay->handleKeyboardFocusChanged(mParent);
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

    mParent->mQDisplay->handleKeyboardFocusChanged(mParent);

    mRepeatTimer.stop();
}

static void sendKey(QWindow *tlw, ulong timestamp, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
                    quint32 nativeScanCode, quint32 nativeVirtualKey, quint32 nativeModifiers,
                    const QString& text = QString(), bool autorep = false, ushort count = 1)
{
    QPlatformInputContext *inputContext = QGuiApplicationPrivate::platformIntegration()->inputContext();
    bool filtered = false;

    if (inputContext) {
        QKeyEvent event(type, key, modifiers, nativeScanCode, nativeVirtualKey, nativeModifiers,
                        text, autorep, count);
        event.setTimestamp(timestamp);
        filtered = inputContext->filterEvent(&event);
    }

    if (!filtered) {
        QWindowSystemInterface::handleExtendedKeyEvent(tlw, timestamp, type, key, modifiers,
                nativeScanCode, nativeVirtualKey, nativeModifiers, text, autorep, count);
    }
}

void QWaylandInputDevice::Keyboard::keyboard_key(uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    QWaylandWindow *window = mFocus;
    uint32_t code = key + 8;
    bool isDown = state != 0;
    QEvent::Type type = isDown ? QEvent::KeyPress : QEvent::KeyRelease;
    QString text;
    int qtkey = key + 8;  // qt-compositor substracts 8 for some reason
    mParent->mSerial = serial;

    if (!window) {
        // We destroyed the keyboard focus surface, but the server
        // didn't get the message yet.
        return;
    }

    if (isDown)
        mParent->mQDisplay->setLastInputDevice(mParent, serial, window);

#if QT_CONFIG(xkbcommon_evdev)
    if (!createDefaultKeyMap()) {
        return;
    }

    const xkb_keysym_t sym = xkb_state_key_get_one_sym(mXkbState, code);

    Qt::KeyboardModifiers modifiers = mParent->modifiers();

    std::tie(qtkey, text) = QWaylandXkb::keysymToQtKey(sym, modifiers);

    sendKey(window->window(), time, type, qtkey, modifiers, code, sym, mNativeModifiers, text);
#else
    // Generic fallback for single hard keys: Assume 'key' is a Qt key code.
    sendKey(window->window(), time, type, qtkey, Qt::NoModifier, code, 0, 0);
#endif

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED
#if QT_CONFIG(xkbcommon_evdev)
        && xkb_keymap_key_repeats(mXkbMap, code)
#endif
        ) {
        mRepeatKey = qtkey;
        mRepeatCode = code;
        mRepeatTime = time;
        mRepeatText = text;
#if QT_CONFIG(xkbcommon_evdev)
        mRepeatSym = sym;
#endif
        mRepeatTimer.setInterval(400);
        mRepeatTimer.start();
    } else if (mRepeatCode == code) {
        mRepeatTimer.stop();
    }
}

void QWaylandInputDevice::Keyboard::repeatKey()
{
    mRepeatTimer.setInterval(25);
    sendKey(mFocus->window(), mRepeatTime, QEvent::KeyRelease, mRepeatKey, modifiers(), mRepeatCode,
#if QT_CONFIG(xkbcommon_evdev)
            mRepeatSym, mNativeModifiers,
#else
            0, 0,
#endif
            mRepeatText, true);

    sendKey(mFocus->window(), mRepeatTime, QEvent::KeyPress, mRepeatKey, modifiers(), mRepeatCode,
#if QT_CONFIG(xkbcommon_evdev)
            mRepeatSym, mNativeModifiers,
#else
            0, 0,
#endif
            mRepeatText, true);
}

void QWaylandInputDevice::Keyboard::keyboard_modifiers(uint32_t serial,
                                             uint32_t mods_depressed,
                                             uint32_t mods_latched,
                                             uint32_t mods_locked,
                                             uint32_t group)
{
    Q_UNUSED(serial);
#if QT_CONFIG(xkbcommon_evdev)
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
    if (!surface)
        return;

    mParent->mTime = time;
    mParent->mSerial = serial;
    mFocus = QWaylandWindow::fromWlSurface(surface);
    mParent->mQDisplay->setLastInputDevice(mParent, serial, mFocus);
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

void QWaylandInputDevice::Touch::releasePoints()
{
    Q_FOREACH (const QWindowSystemInterface::TouchPoint &previousPoint, mPrevTouchPoints) {
        QWindowSystemInterface::TouchPoint tp = previousPoint;
        tp.state = Qt::TouchPointReleased;
        mTouchPoints.append(tp);
    }
    touch_frame();
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

}

QT_END_NAMESPACE
