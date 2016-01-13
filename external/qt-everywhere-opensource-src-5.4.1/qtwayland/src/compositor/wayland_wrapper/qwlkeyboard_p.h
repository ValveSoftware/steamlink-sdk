/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidi ary(-ies).
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

#ifndef QTWAYLAND_QWLKEYBOARD_P_H
#define QTWAYLAND_QWLKEYBOARD_P_H

#include <QtCompositor/qwaylandexport.h>
#include <QtCompositor/qwaylandinput.h>

#include <QObject>
#include <QtCompositor/private/qwayland-server-wayland.h>

#include <QtCore/QVector>

#ifndef QT_NO_WAYLAND_XKB
#include <xkbcommon/xkbcommon.h>
#endif

#include "qwllistener_p.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

class Compositor;
class InputDevice;
class Surface;
class Keyboard;

class Q_COMPOSITOR_EXPORT KeyboardGrabber {
    public:
        virtual ~KeyboardGrabber();
        virtual void focused(Surface *surface) = 0;
        virtual void key(uint32_t serial, uint32_t time, uint32_t key, uint32_t state) = 0;
        virtual void modifiers(uint32_t serial, uint32_t mods_depressed,
                uint32_t mods_latched, uint32_t mods_locked, uint32_t group) = 0;

        Keyboard *m_keyboard;
};

class Q_COMPOSITOR_EXPORT Keyboard : public QObject, public QtWaylandServer::wl_keyboard, public KeyboardGrabber
{
    Q_OBJECT

public:
    Keyboard(Compositor *compositor, InputDevice *seat);
    ~Keyboard();

    void setFocus(Surface *surface);
    void setKeymap(const QWaylandKeymap &keymap);

    void sendKeyModifiers(Resource *resource, uint32_t serial);
    void sendKeyPressEvent(uint code);
    void sendKeyReleaseEvent(uint code);

    Surface *focus() const;
    Resource *focusResource() const;

    void focused(Surface* surface);
    void key(uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
    void modifiers(uint32_t serial, uint32_t mods_depressed,
                uint32_t mods_latched, uint32_t mods_locked, uint32_t group);

   void startGrab(KeyboardGrabber *grab);
   void endGrab();
   KeyboardGrabber *currentGrab() const;


Q_SIGNALS:
    void focusChanged(Surface *surface);

protected:
    void keyboard_bind_resource(Resource *resource);
    void keyboard_destroy_resource(Resource *resource);

private:
    void sendKeyEvent(uint code, uint32_t state);
    void updateModifierState(uint code, uint32_t state);
    void updateKeymap();
    void focusDestroyed(void *data);

#ifndef QT_NO_WAYLAND_XKB
    void initXKB();
    void createXKBKeymap();
#endif

    Compositor *m_compositor;
    InputDevice *m_seat;

    KeyboardGrabber* m_grab;
    Surface *m_focus;
    Resource *m_focusResource;
    WlListener m_focusDestroyListener;

    QVector<uint32_t> m_keys;
    uint32_t m_modsDepressed;
    uint32_t m_modsLatched;
    uint32_t m_modsLocked;
    uint32_t m_group;

    QWaylandKeymap m_keymap;
    bool m_pendingKeymap;
#ifndef QT_NO_WAYLAND_XKB
    size_t m_keymap_size;
    int m_keymap_fd;
    char *m_keymap_area;
    struct xkb_context *m_context;
    struct xkb_state *m_state;
#endif
};

} // namespace QtWayland

QT_END_NAMESPACE

#endif // QTWAYLAND_QWLKEYBOARD_P_H
