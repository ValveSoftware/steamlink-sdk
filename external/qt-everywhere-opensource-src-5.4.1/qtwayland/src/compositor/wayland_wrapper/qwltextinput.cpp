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

#include "qwltextinput_p.h"

#include "qwlcompositor_p.h"
#include "qwlinputdevice_p.h"
#include "qwlinputmethod_p.h"
#include "qwlinputmethodcontext_p.h"
#include "qwlinputpanel_p.h"
#include "qwlsurface_p.h"

#include <algorithm>

QT_BEGIN_NAMESPACE

namespace QtWayland {

TextInput::TextInput(Compositor *compositor, struct ::wl_client *client, int id)
    : wl_text_input(client, id, 1)
    , m_compositor(compositor)
    , m_focus()
    , m_inputPanelVisible()
    , m_cursorRectangle()
{
}

Surface *TextInput::focus() const
{
    return m_focus;
}

bool TextInput::inputPanelVisible() const
{
    return m_inputPanelVisible;
}

QRect TextInput::cursorRectangle() const
{
    return m_cursorRectangle;
}

void TextInput::deactivate(InputMethod *inputMethod)
{
    if (m_activeInputMethods.removeOne(inputMethod))
        inputMethod->deactivate();

    if (m_activeInputMethods.isEmpty())
        send_leave();
}

void TextInput::text_input_destroy_resource(Resource *)
{
    Q_FOREACH (InputMethod *inputMethod, m_activeInputMethods) {
        deactivate(inputMethod);
    }

    delete this;
}

void TextInput::text_input_activate(Resource *, wl_resource *seat, wl_resource *surface)
{
    Surface *oldSurface = m_focus;
    m_focus = Surface::fromResource(surface);

    if (oldSurface != m_focus)
        send_leave();

    bool wasEmpty = m_activeInputMethods.isEmpty();

    InputMethod *inputMethod = InputDevice::fromSeatResource(seat)->inputMethod();

    if (!m_activeInputMethods.contains(inputMethod)) {
        m_activeInputMethods.append(inputMethod);
        inputMethod->activate(this);
    }

    if (wasEmpty || oldSurface != m_focus)
        send_enter(surface);
}

void TextInput::text_input_deactivate(Resource *, wl_resource *seat)
{
    InputMethod *inputMethod = InputDevice::fromSeatResource(seat)->inputMethod();

    deactivate(inputMethod);
}

static bool isInputMethodBound(InputMethod *inputMethod)
{
    return inputMethod->isBound();
}

void TextInput::text_input_show_input_panel(Resource *)
{
    m_inputPanelVisible = true;

    if (std::find_if(m_activeInputMethods.cbegin(), m_activeInputMethods.cend(), isInputMethodBound) != m_activeInputMethods.cend())
        m_compositor->inputPanel()->setInputPanelVisible(true);
}

void TextInput::text_input_hide_input_panel(Resource *)
{
    m_inputPanelVisible = false;

    if (std::find_if(m_activeInputMethods.cbegin(), m_activeInputMethods.cend(), isInputMethodBound) != m_activeInputMethods.cend())
        m_compositor->inputPanel()->setInputPanelVisible(false);
}

void TextInput::text_input_set_cursor_rectangle(Resource *, int32_t x, int32_t y, int32_t width, int32_t height)
{
    m_cursorRectangle = QRect(x, y, width, height);

    if (!m_activeInputMethods.isEmpty())
        m_compositor->inputPanel()->setCursorRectangle(m_cursorRectangle);
}

void TextInput::text_input_reset(Resource *)
{
    Q_FOREACH (InputMethod *inputMethod, m_activeInputMethods) {
        if (inputMethod->context())
            inputMethod->context()->send_reset();
    }
}

void TextInput::text_input_commit_state(Resource *, uint32_t serial)
{
    Q_FOREACH (InputMethod *inputMethod, m_activeInputMethods) {
        if (inputMethod->context())
            inputMethod->context()->send_commit_state(serial);
    }
}

void TextInput::text_input_set_content_type(Resource *, uint32_t hint, uint32_t purpose)
{
    Q_FOREACH (InputMethod *inputMethod, m_activeInputMethods) {
        if (inputMethod->context())
            inputMethod->context()->send_content_type(hint, purpose);
    }
}

void TextInput::text_input_set_preferred_language(Resource *, const QString &language)
{
    Q_FOREACH (InputMethod *inputMethod, m_activeInputMethods) {
        if (inputMethod->context())
            inputMethod->context()->send_preferred_language(language);
    }
}

void TextInput::text_input_set_surrounding_text(Resource *, const QString &text, uint32_t cursor, uint32_t anchor)
{
    Q_FOREACH (InputMethod *inputMethod, m_activeInputMethods) {
        if (inputMethod->context())
            inputMethod->context()->send_surrounding_text(text, cursor, anchor);
    }
}

void TextInput::text_input_invoke_action(Resource *, uint32_t button, uint32_t index)
{
    Q_FOREACH (InputMethod *inputMethod, m_activeInputMethods) {
        if (inputMethod->context())
            inputMethod->context()->send_invoke_action(button, index);
    }
}

} // namespace QtWayland

QT_END_NAMESPACE
