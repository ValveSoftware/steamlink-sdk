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

#include "qwlinputmethodcontext_p.h"

#include "qwltextinput_p.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

InputMethodContext::InputMethodContext(wl_client *client, TextInput *textInput)
    : QtWaylandServer::wl_input_method_context(client, 0, 1)
    , m_textInput(textInput)
{
}

InputMethodContext::~InputMethodContext()
{
}

void InputMethodContext::input_method_context_destroy_resource(Resource *)
{
    delete this;
}

void InputMethodContext::input_method_context_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void InputMethodContext::input_method_context_commit_string(Resource *, uint32_t serial, const QString &text)
{
    m_textInput->send_commit_string(serial, text);
}

void InputMethodContext::input_method_context_cursor_position(Resource *, int32_t index, int32_t anchor)
{
    m_textInput->send_cursor_position(index, anchor);
}

void InputMethodContext::input_method_context_delete_surrounding_text(Resource *, int32_t index, uint32_t length)
{
    m_textInput->send_delete_surrounding_text(index, length);
}

void InputMethodContext::input_method_context_language(Resource *, uint32_t serial, const QString &language)
{
    m_textInput->send_language(serial, language);
}

void InputMethodContext::input_method_context_keysym(Resource *, uint32_t serial, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers)
{
    m_textInput->send_keysym(serial, time, sym, state, modifiers);
}

void InputMethodContext::input_method_context_modifiers_map(Resource *, wl_array *map)
{
    QByteArray modifiersArray(static_cast<char *>(map->data), map->size);
    m_textInput->send_modifiers_map(modifiersArray);
}

void InputMethodContext::input_method_context_preedit_cursor(Resource *, int32_t index)
{
    m_textInput->send_preedit_cursor(index);
}

void InputMethodContext::input_method_context_preedit_string(Resource *, uint32_t serial, const QString &text, const QString &commit)
{
    m_textInput->send_preedit_string(serial, text, commit);
}

void InputMethodContext::input_method_context_preedit_styling(Resource *, uint32_t index, uint32_t length, uint32_t style)
{
    m_textInput->send_preedit_styling(index, length, style);
}

void InputMethodContext::input_method_context_grab_keyboard(Resource *, uint32_t keyboard)
{
    Q_UNUSED(keyboard);
}

void InputMethodContext::input_method_context_key(Resource *, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    Q_UNUSED(serial);
    Q_UNUSED(time);
    Q_UNUSED(key);
    Q_UNUSED(state);
}

void InputMethodContext::input_method_context_modifiers(Resource *, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    Q_UNUSED(serial);
    Q_UNUSED(mods_depressed);
    Q_UNUSED(mods_latched);
    Q_UNUSED(mods_locked);
    Q_UNUSED(group);
}

} // namespace QtWayland

QT_END_NAMESPACE
