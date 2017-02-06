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

#include "qwaylandseat.h"
#include "qwaylandseat_p.h"

#include "qwaylandcompositor.h"
#include "qwaylandinputmethodcontrol.h"
#include "qwaylandview.h"
#include <QtWaylandCompositor/QWaylandDrag>
#include <QtWaylandCompositor/QWaylandTouch>
#include <QtWaylandCompositor/QWaylandPointer>
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/private/qwaylandseat_p.h>
#include <QtWaylandCompositor/private/qwaylandcompositor_p.h>
#include <QtWaylandCompositor/private/qwldatadevice_p.h>

#include "extensions/qwlqtkey_p.h"
#include "extensions/qwaylandtextinput.h"

QT_BEGIN_NAMESPACE

QWaylandSeatPrivate::QWaylandSeatPrivate(QWaylandSeat *seat)
    : QObjectPrivate()
    , QtWaylandServer::wl_seat()
    , isInitialized(false)
    , compositor(nullptr)
    , mouseFocus(Q_NULLPTR)
    , keyboardFocus(nullptr)
    , capabilities()
    , data_device()
    , drag_handle(new QWaylandDrag(seat))
    , keymap(new QWaylandKeymap())
{
}

QWaylandSeatPrivate::~QWaylandSeatPrivate()
{
}

void QWaylandSeatPrivate::setCapabilities(QWaylandSeat::CapabilityFlags caps)
{
    Q_Q(QWaylandSeat);
    if (capabilities != caps) {
        QWaylandSeat::CapabilityFlags changed = caps ^ capabilities;

        if (changed & QWaylandSeat::Pointer) {
            pointer.reset(pointer.isNull() ? QWaylandCompositorPrivate::get(compositor)->callCreatePointerDevice(q) : 0);
        }

        if (changed & QWaylandSeat::Keyboard) {
            keyboard.reset(keyboard.isNull() ? QWaylandCompositorPrivate::get(compositor)->callCreateKeyboardDevice(q) : 0);
        }

        if (changed & QWaylandSeat::Touch) {
            touch.reset(touch.isNull() ? QWaylandCompositorPrivate::get(compositor)->callCreateTouchDevice(q) : 0);
        }

        capabilities = caps;
        QList<Resource *> resources = resourceMap().values();
        for (int i = 0; i < resources.size(); i++) {
            wl_seat::send_capabilities(resources.at(i)->handle, (uint32_t)capabilities);
        }

        if ((changed & caps & QWaylandSeat::Keyboard) && keyboardFocus != nullptr)
            keyboard->setFocus(keyboardFocus);
    }
}

void QWaylandSeatPrivate::clientRequestedDataDevice(QtWayland::DataDeviceManager *, struct wl_client *client, uint32_t id)
{
    Q_Q(QWaylandSeat);
    if (!data_device)
        data_device.reset(new QtWayland::DataDevice(q));
    data_device->add(client, id, 1);
}

void QWaylandSeatPrivate::seat_destroy_resource(wl_seat::Resource *)
{
//    cleanupDataDeviceForClient(resource->client(), true);
}

void QWaylandSeatPrivate::seat_bind_resource(wl_seat::Resource *resource)
{
    // The order of capabilities matches the order defined in the wayland protocol
    wl_seat::send_capabilities(resource->handle, (uint32_t)capabilities);
}

void QWaylandSeatPrivate::seat_get_pointer(wl_seat::Resource *resource, uint32_t id)
{
    if (!pointer.isNull()) {
        pointer->addClient(QWaylandClient::fromWlClient(compositor, resource->client()), id, resource->version());
    }
}

void QWaylandSeatPrivate::seat_get_keyboard(wl_seat::Resource *resource, uint32_t id)
{
    if (!keyboard.isNull()) {
        keyboard->addClient(QWaylandClient::fromWlClient(compositor, resource->client()), id, resource->version());
    }
}

void QWaylandSeatPrivate::seat_get_touch(wl_seat::Resource *resource, uint32_t id)
{
    if (!touch.isNull()) {
        touch->addClient(QWaylandClient::fromWlClient(compositor, resource->client()), id, resource->version());
    }
}

/*!
 * \class QWaylandSeat
 * \inmodule QtWaylandCompositor
 * \since 5.8
 * \brief The QWaylandSeat class provides access to keyboard, mouse, and touch input.
 *
 * The QWaylandSeat provides access to different types of user input and maintains
 * a keyboard focus and a mouse pointer. It corresponds to the wl_seat interface in the Wayland protocol.
 */

/*!
 * \enum QWaylandSeat::CapabilityFlag
 *
 * This enum type describes the capabilities of a QWaylandSeat.
 *
 * \value Pointer The QWaylandSeat supports pointer input.
 * \value Keyboard The QWaylandSeat supports keyboard input.
 * \value Touch The QWaylandSeat supports touch input.
 */

/*!
 * Constructs a QWaylandSeat for the given \a compositor and with the given \a capabilityFlags.
 */
QWaylandSeat::QWaylandSeat(QWaylandCompositor *compositor, CapabilityFlags capabilityFlags)
    : QWaylandObject(*new QWaylandSeatPrivate(this))
{
    Q_D(QWaylandSeat);
    d->compositor = compositor;
    d->capabilities = capabilityFlags;
    if (compositor->isCreated())
        initialize();
}

/*!
 * Destroys the QWaylandSeat
 */
QWaylandSeat::~QWaylandSeat()
{
}

void QWaylandSeat::initialize()
{
    Q_D(QWaylandSeat);
    d->init(d->compositor->display(), 4);

    if (d->capabilities & QWaylandSeat::Pointer)
        d->pointer.reset(QWaylandCompositorPrivate::get(d->compositor)->callCreatePointerDevice(this));
    if (d->capabilities & QWaylandSeat::Touch)
        d->touch.reset(QWaylandCompositorPrivate::get(d->compositor)->callCreateTouchDevice(this));
    if (d->capabilities & QWaylandSeat::Keyboard)
        d->keyboard.reset(QWaylandCompositorPrivate::get(d->compositor)->callCreateKeyboardDevice(this));

    d->isInitialized = true;
}

bool QWaylandSeat::isInitialized() const
{
    Q_D(const QWaylandSeat);
    return d->isInitialized;
}

/*!
 * Sends a mouse press event for \a button to the QWaylandSeat's pointer device.
 */
void QWaylandSeat::sendMousePressEvent(Qt::MouseButton button)
{
    Q_D(QWaylandSeat);
    d->pointer->sendMousePressEvent(button);
}

/*!
 * Sends a mouse release event for \a button to the QWaylandSeat's pointer device.
 */
void QWaylandSeat::sendMouseReleaseEvent(Qt::MouseButton button)
{
    Q_D(QWaylandSeat);
    d->pointer->sendMouseReleaseEvent(button);
}

/*!
 * Sets the mouse focus to \a view and sends a mouse move event to the pointer device with the
 * local position \a localPos and output space position \a outputSpacePos.
 **/
void QWaylandSeat::sendMouseMoveEvent(QWaylandView *view, const QPointF &localPos, const QPointF &outputSpacePos)
{
    Q_D(QWaylandSeat);
    d->pointer->sendMouseMoveEvent(view, localPos, outputSpacePos);
}

/*!
 * Sends a mouse wheel event to the QWaylandSeat's pointer device with the given \a orientation and \a delta.
 */
void QWaylandSeat::sendMouseWheelEvent(Qt::Orientation orientation, int delta)
{
    Q_D(QWaylandSeat);
    d->pointer->sendMouseWheelEvent(orientation, delta);
}

/*!
 * Sends a key press event with the key \a code to the keyboard device.
 */
void QWaylandSeat::sendKeyPressEvent(uint code)
{
    Q_D(QWaylandSeat);
    d->keyboard->sendKeyPressEvent(code);
}

/*!
 * Sends a key release event with the key \a code to the keyboard device.
 */
void QWaylandSeat::sendKeyReleaseEvent(uint code)
{
    Q_D(QWaylandSeat);
    d->keyboard->sendKeyReleaseEvent(code);
}

/*!
 * Sends a touch point event to the \a surface on a touch device with the given
 * \a id, \a point and \a state.
 *
 * Returns the serial for the touch up or touch down event.
 */
uint QWaylandSeat::sendTouchPointEvent(QWaylandSurface *surface, int id, const QPointF &point, Qt::TouchPointState state)
{
    Q_D(QWaylandSeat);

    if (d->touch.isNull())
        return 0;

    return d->touch->sendTouchPointEvent(surface, id, point,state);
}

/*!
 * Sends a frame event to the touch device of a \a client.
 */
void QWaylandSeat::sendTouchFrameEvent(QWaylandClient *client)
{
    Q_D(QWaylandSeat);
    if (!d->touch.isNull())
        d->touch->sendFrameEvent(client);
}

/*!
 * Sends a cancel event to the touch device of a \a client.
 */
void QWaylandSeat::sendTouchCancelEvent(QWaylandClient *client)
{
    Q_D(QWaylandSeat);
    if (!d->touch.isNull())
        d->touch->sendCancelEvent(client);
}

/*!
 * Sends the \a event to the specified \a surface on the touch device.
 */
void QWaylandSeat::sendFullTouchEvent(QWaylandSurface *surface, QTouchEvent *event)
{
    Q_D(QWaylandSeat);

    if (!d->touch)
        return;

    d->touch->sendFullTouchEvent(surface, event);
}

/*!
 * Sends the \a event to the keyboard device.
 */
void QWaylandSeat::sendFullKeyEvent(QKeyEvent *event)
{
    Q_D(QWaylandSeat);

    if (!keyboardFocus()) {
        qWarning("Cannot send key event, no keyboard focus, fix the compositor");
        return;
    }

    if (keyboardFocus()->inputMethodControl()->enabled()
        && event->nativeScanCode() == 0) {
        QWaylandTextInput *textInput = QWaylandTextInput::findIn(this);
        if (textInput) {
            textInput->sendKeyEvent(event);
            return;
        }
    }

    QtWayland::QtKeyExtensionGlobal *ext = QtWayland::QtKeyExtensionGlobal::findIn(d->compositor);
    if (ext && ext->postQtKeyEvent(event, keyboardFocus()))
        return;

    if (!d->keyboard.isNull() && !event->isAutoRepeat()) {
        if (event->type() == QEvent::KeyPress)
            d->keyboard->sendKeyPressEvent(event->nativeScanCode());
        else if (event->type() == QEvent::KeyRelease)
            d->keyboard->sendKeyReleaseEvent(event->nativeScanCode());
    }
}

/*!
 * Returns the keyboard for this input device.
 */
QWaylandKeyboard *QWaylandSeat::keyboard() const
{
    Q_D(const QWaylandSeat);
    return d->keyboard.data();
}

/*!
 * Returns the current focused surface for keyboard input.
 */
QWaylandSurface *QWaylandSeat::keyboardFocus() const
{
    Q_D(const QWaylandSeat);
    if (d->keyboard.isNull() || !d->keyboard->focus())
        return Q_NULLPTR;

    return d->keyboard->focus();
}

/*!
 * Sets the current keyboard focus to \a surface.
 */
bool QWaylandSeat::setKeyboardFocus(QWaylandSurface *surface)
{
    Q_D(QWaylandSeat);
    if (surface && surface->isDestroyed())
        return false;

    QWaylandSurface *oldSurface = keyboardFocus();
    if (surface == oldSurface)
        return true;

    d->keyboardFocus = surface;
    if (!d->keyboard.isNull())
        d->keyboard->setFocus(surface);
    if (d->data_device)
        d->data_device->setFocus(surface->client());
    emit keyboardFocusChanged(surface, oldSurface);
    return true;
}

QWaylandKeymap *QWaylandSeat::keymap()
{
    Q_D(const QWaylandSeat);
    return d->keymap.data();
}

/*!
 * Returns the pointer device for this QWaylandSeat.
 */
QWaylandPointer *QWaylandSeat::pointer() const
{
    Q_D(const QWaylandSeat);
    return d->pointer.data();
}

/*!
 * Returns the touch device for this QWaylandSeat.
 */
QWaylandTouch *QWaylandSeat::touch() const
{
    Q_D(const QWaylandSeat);
    return d->touch.data();
}

/*!
 * Returns the view that currently has mouse focus.
 */
QWaylandView *QWaylandSeat::mouseFocus() const
{
    Q_D(const QWaylandSeat);
    return d->mouseFocus;
}

/*!
 * Sets the current mouse focus to \a view.
 */
void QWaylandSeat::setMouseFocus(QWaylandView *view)
{
    Q_D(QWaylandSeat);
    if (view == d->mouseFocus)
        return;

    QWaylandView *oldFocus = d->mouseFocus;
    d->mouseFocus = view;
    emit mouseFocusChanged(d->mouseFocus, oldFocus);
}

/*!
 * Returns the compositor for this QWaylandSeat.
 */
QWaylandCompositor *QWaylandSeat::compositor() const
{
    Q_D(const QWaylandSeat);
    return d->compositor;
}

/*!
 * Returns the drag object for this QWaylandSeat.
 */
QWaylandDrag *QWaylandSeat::drag() const
{
    Q_D(const QWaylandSeat);
    return d->drag_handle.data();
}

/*!
 * Returns the capability flags for this QWaylandSeat.
 */
QWaylandSeat::CapabilityFlags QWaylandSeat::capabilities() const
{
    Q_D(const QWaylandSeat);
    return d->capabilities;
}

/*!
 * \internal
 */
bool QWaylandSeat::isOwner(QInputEvent *inputEvent) const
{
    Q_UNUSED(inputEvent);
    return true;
}

/*!
 * Returns the QWaylandSeat corresponding to the \a resource. The \a resource is expected
 * to have the type wl_seat.
 */
QWaylandSeat *QWaylandSeat::fromSeatResource(struct ::wl_resource *resource)
{
    return static_cast<QWaylandSeatPrivate *>(QWaylandSeatPrivate::Resource::fromResource(resource)->seat_object)->q_func();
}

/*!
 * \fn void QWaylandSeat::mouseFocusChanged(QWaylandView *newFocus, QWaylandView *oldFocus)
 *
 * This signal is emitted when the mouse focus has changed from \a oldFocus to \a newFocus.
 */

QT_END_NAMESPACE
