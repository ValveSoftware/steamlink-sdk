/****************************************************************************
**
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

#ifndef QTWAYLAND_QWLINPUTMETHODCONTEXT_P_H
#define QTWAYLAND_QWLINPUTMETHODCONTEXT_P_H

#include <QtCompositor/private/qwayland-server-input-method.h>

QT_BEGIN_NAMESPACE

namespace QtWayland {

class TextInput;

class InputMethodContext : public QtWaylandServer::wl_input_method_context
{
public:
    explicit InputMethodContext(struct ::wl_client *client, TextInput *textInput);
    ~InputMethodContext();

protected:
    void input_method_context_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;
    void input_method_context_destroy(Resource *resource) Q_DECL_OVERRIDE;

    void input_method_context_commit_string(Resource *resource, uint32_t serial, const QString &text) Q_DECL_OVERRIDE;
    void input_method_context_cursor_position(Resource *resource, int32_t index, int32_t anchor) Q_DECL_OVERRIDE;
    void input_method_context_delete_surrounding_text(Resource *resource, int32_t index, uint32_t length) Q_DECL_OVERRIDE;
    void input_method_context_language(Resource *resource, uint32_t serial, const QString &language) Q_DECL_OVERRIDE;
    void input_method_context_keysym(Resource *resource, uint32_t serial, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers) Q_DECL_OVERRIDE;
    void input_method_context_modifiers_map(Resource *resource, wl_array *map) Q_DECL_OVERRIDE;
    void input_method_context_preedit_cursor(Resource *resource, int32_t index) Q_DECL_OVERRIDE;
    void input_method_context_preedit_string(Resource *resource, uint32_t serial, const QString &text, const QString &commit) Q_DECL_OVERRIDE;
    void input_method_context_preedit_styling(Resource *resource, uint32_t index, uint32_t length, uint32_t style) Q_DECL_OVERRIDE;
    void input_method_context_grab_keyboard(Resource *resource, uint32_t keyboard) Q_DECL_OVERRIDE;
    void input_method_context_key(Resource *resource, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) Q_DECL_OVERRIDE;
    void input_method_context_modifiers(Resource *resource, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) Q_DECL_OVERRIDE;

private:
    TextInput *m_textInput;
};

} // namespace QtWayland

QT_END_NAMESPACE

#endif // QTWAYLAND_QWLINPUTMETHODCONTEXT_P_H
