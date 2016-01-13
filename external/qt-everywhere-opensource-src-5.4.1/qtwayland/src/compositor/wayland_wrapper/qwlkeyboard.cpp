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

#include "qwlkeyboard_p.h"

#include <QFile>
#include <QStandardPaths>

#include "qwlcompositor_p.h"
#include "qwlsurface_p.h"

#include <fcntl.h>
#include <unistd.h>
#ifndef QT_NO_WAYLAND_XKB
#include <sys/mman.h>
#include <sys/types.h>
#endif

QT_BEGIN_NAMESPACE

namespace QtWayland {

Keyboard::Keyboard(Compositor *compositor, InputDevice *seat)
    : QtWaylandServer::wl_keyboard()
    , m_compositor(compositor)
    , m_seat(seat)
    , m_grab(this)
    , m_focus()
    , m_focusResource()
    , m_keys()
    , m_modsDepressed()
    , m_modsLatched()
    , m_modsLocked()
    , m_group()
    , m_pendingKeymap(false)
#ifndef QT_NO_WAYLAND_XKB
    , m_state(0)
#endif
{
#ifndef QT_NO_WAYLAND_XKB
    initXKB();
#endif
    connect(&m_focusDestroyListener, &WlListener::fired, this, &Keyboard::focusDestroyed);
}

Keyboard::~Keyboard()
{
#ifndef QT_NO_WAYLAND_XKB
    if (m_context) {
        if (m_keymap_area)
            munmap(m_keymap_area, m_keymap_size);
        close(m_keymap_fd);
        xkb_context_unref(m_context);
        xkb_state_unref(m_state);
    }
#endif
}

KeyboardGrabber::~KeyboardGrabber()
{
}

void Keyboard::startGrab(KeyboardGrabber *grab)
{
    m_grab = grab;
    m_grab->m_keyboard = this;
    m_grab->focused(m_focus);
}

void Keyboard::endGrab()
{
    m_grab = this;
}

KeyboardGrabber *Keyboard::currentGrab() const
{
    return m_grab;
}

void Keyboard::focused(Surface *surface)
{
    if (m_focusResource && m_focus != surface) {
        uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
        send_leave(m_focusResource->handle, serial, m_focus->resource()->handle);
        m_focusDestroyListener.reset();
    }

    Resource *resource = surface ? resourceMap().value(surface->resource()->client()) : 0;

    if (resource && (m_focus != surface || m_focusResource != resource)) {
        uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
        send_modifiers(resource->handle, serial, m_modsDepressed, m_modsLatched, m_modsLocked, m_group);
        send_enter(resource->handle, serial, surface->resource()->handle, QByteArray::fromRawData((char *)m_keys.data(), m_keys.size() * sizeof(uint32_t)));
        m_focusDestroyListener.listenForDestruction(surface->resource()->handle);
    }

    m_focusResource = resource;
    m_focus = surface;
    Q_EMIT focusChanged(m_focus);
}

void Keyboard::setFocus(Surface* surface)
{
    m_grab->focused(surface);
}

void Keyboard::setKeymap(const QWaylandKeymap &keymap)
{
    m_keymap = keymap;
    m_pendingKeymap = true;
}

void Keyboard::focusDestroyed(void *data)
{
    Q_UNUSED(data)
    m_focusDestroyListener.reset();

    m_focus = 0;
    m_focusResource = 0;
}

void Keyboard::sendKeyModifiers(wl_keyboard::Resource *resource, uint32_t serial)
{
    send_modifiers(resource->handle, serial, m_modsDepressed, m_modsLatched, m_modsLocked, m_group);
}

void Keyboard::sendKeyPressEvent(uint code)
{
    sendKeyEvent(code, WL_KEYBOARD_KEY_STATE_PRESSED);
}

void Keyboard::sendKeyReleaseEvent(uint code)
{
    sendKeyEvent(code, WL_KEYBOARD_KEY_STATE_RELEASED);
}

Surface *Keyboard::focus() const
{
    return m_focus;
}

QtWaylandServer::wl_keyboard::Resource *Keyboard::focusResource() const
{
    return m_focusResource;
}

void Keyboard::keyboard_bind_resource(wl_keyboard::Resource *resource)
{
#ifndef QT_NO_WAYLAND_XKB
    if (m_context) {
        send_keymap(resource->handle, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                    m_keymap_fd, m_keymap_size);
        return;
    }
#endif
    int null_fd = open("/dev/null", O_RDONLY);
    send_keymap(resource->handle, 0 /* WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP */,
                null_fd, 0);
    close(null_fd);
}

void Keyboard::keyboard_destroy_resource(wl_keyboard::Resource *resource)
{
    if (m_focusResource == resource)
        m_focusResource = 0;
}

void Keyboard::key(uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    if (m_focusResource) {
        send_key(m_focusResource->handle, serial, time, key, state);
    }
}

void Keyboard::sendKeyEvent(uint code, uint32_t state)
{
    // There must be no keys pressed when changing the keymap,
    // see http://lists.freedesktop.org/archives/wayland-devel/2013-October/011395.html
    if (m_pendingKeymap && m_keys.isEmpty())
        updateKeymap();

    uint32_t time = m_compositor->currentTimeMsecs();
    uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
    uint key = code - 8;
    m_grab->key(serial, time, key, state);
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        m_keys << key;
    } else {
        for (int i = 0; i < m_keys.size(); ++i) {
            if (m_keys.at(i) == key) {
                m_keys.remove(i);
            }
        }
    }
    updateModifierState(code, state);
}

void Keyboard::modifiers(uint32_t serial, uint32_t mods_depressed,
                         uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    if (m_focusResource) {
        send_modifiers(m_focusResource->handle, serial, mods_depressed, mods_latched, mods_locked, group);
    }
}

void Keyboard::updateModifierState(uint code, uint32_t state)
{
#ifndef QT_NO_WAYLAND_XKB
    if (!m_context)
        return;

    xkb_state_update_key(m_state, code, state == WL_KEYBOARD_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP);

    uint32_t modsDepressed = xkb_state_serialize_mods(m_state, (xkb_state_component)XKB_STATE_DEPRESSED);
    uint32_t modsLatched = xkb_state_serialize_mods(m_state, (xkb_state_component)XKB_STATE_LATCHED);
    uint32_t modsLocked = xkb_state_serialize_mods(m_state, (xkb_state_component)XKB_STATE_LOCKED);
    uint32_t group = xkb_state_serialize_group(m_state, (xkb_state_component)XKB_STATE_EFFECTIVE);

    if (modsDepressed == m_modsDepressed
            && modsLatched == m_modsLatched
            && modsLocked == m_modsLocked
            && group == m_group)
        return;

    m_modsDepressed = modsDepressed;
    m_modsLatched = modsLatched;
    m_modsLocked = modsLocked;
    m_group = group;

    m_grab->modifiers(wl_display_next_serial(m_compositor->wl_display()), m_modsDepressed, m_modsLatched, m_modsLocked, m_group);
#else
    Q_UNUSED(code);
    Q_UNUSED(state);
#endif
}

void Keyboard::updateKeymap()
{
    m_pendingKeymap = false;
#ifndef QT_NO_WAYLAND_XKB
    if (!m_context)
        return;

    createXKBKeymap();
    foreach (Resource *res, resourceMap()) {
        send_keymap(res->handle, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, m_keymap_fd, m_keymap_size);
    }

    xkb_state_update_mask(m_state, 0, m_modsLatched, m_modsLocked, 0, 0, 0);
    if (m_focusResource)
        sendKeyModifiers(m_focusResource, wl_display_next_serial(m_compositor->wl_display()));
#endif
}

#ifndef QT_NO_WAYLAND_XKB
static int createAnonymousFile(size_t size)
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (path.isEmpty())
        return -1;

    QByteArray name = QFile::encodeName(path + QStringLiteral("/qtwayland-XXXXXX"));

    int fd = mkstemp(name.data());
    if (fd < 0)
        return -1;

    long flags = fcntl(fd, F_GETFD);
    if (flags == -1 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(fd);
        fd = -1;
    }
    unlink(name.constData());

    if (fd < 0)
        return -1;

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

void Keyboard::initXKB()
{
    m_context = xkb_context_new(static_cast<xkb_context_flags>(0));
    if (!m_context) {
        qWarning("Failed to create a XKB context: keymap will not be supported");
        return;
    }

    createXKBKeymap();
}

void Keyboard::createXKBKeymap()
{
    if (!m_context)
        return;

    if (m_state)
        xkb_state_unref(m_state);

    struct xkb_rule_names rule_names = { strdup(qPrintable(m_keymap.rules())),
                                         strdup(qPrintable(m_keymap.model())),
                                         strdup(qPrintable(m_keymap.layout())),
                                         strdup(qPrintable(m_keymap.variant())),
                                         strdup(qPrintable(m_keymap.options())) };
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(m_context, &rule_names, static_cast<xkb_keymap_compile_flags>(0));

    char *keymap_str = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    if (!keymap_str)
        qFatal("Failed to compile global XKB keymap");

    m_keymap_size = strlen(keymap_str) + 1;
    m_keymap_fd = createAnonymousFile(m_keymap_size);
    if (m_keymap_fd < 0)
        qFatal("Failed to create anonymous file of size %lu", static_cast<unsigned long>(m_keymap_size));

    m_keymap_area = static_cast<char *>(mmap(0, m_keymap_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_keymap_fd, 0));
    if (m_keymap_area == MAP_FAILED) {
        close(m_keymap_fd);
        qFatal("Failed to map shared memory segment");
    }

    strcpy(m_keymap_area, keymap_str);

    m_state = xkb_state_new(keymap);

    xkb_keymap_unref(keymap);

    free((char *)rule_names.rules);
    free((char *)rule_names.model);
    free((char *)rule_names.layout);
    free((char *)rule_names.variant);
    free((char *)rule_names.options);
}
#endif

} // namespace QtWayland

QT_END_NAMESPACE
