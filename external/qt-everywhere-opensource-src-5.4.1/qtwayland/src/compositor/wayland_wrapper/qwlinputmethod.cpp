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

#include "qwlinputmethod_p.h"

#include "qwlcompositor_p.h"
#include "qwlinputdevice_p.h"
#include "qwlinputmethodcontext_p.h"
#include "qwlinputpanel_p.h"
#include "qwlkeyboard_p.h"
#include "qwltextinput_p.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

InputMethod::InputMethod(Compositor *compositor, InputDevice *seat)
    : QtWaylandServer::wl_input_method(seat->compositor()->wl_display(), 1)
    , m_compositor(compositor)
    , m_seat(seat)
    , m_resource(0)
    , m_textInput()
    , m_context()
{
    connect(seat->keyboardDevice(), SIGNAL(focusChanged(Surface*)), this, SLOT(focusChanged(Surface*)));
}

InputMethod::~InputMethod()
{
}

void InputMethod::activate(TextInput *textInput)
{
    if (!m_resource) {
        qDebug("Cannot activate (no input method running).");
        return;
    }

    if (m_textInput) {
        Q_ASSERT(m_textInput != textInput);
        m_textInput->deactivate(this);
    }
    m_textInput = textInput;
    m_context = new InputMethodContext(m_resource->client(), textInput);

    send_activate(m_resource->handle, m_context->resource()->handle);

    m_compositor->inputPanel()->setFocus(textInput->focus());
    m_compositor->inputPanel()->setCursorRectangle(textInput->cursorRectangle());
    m_compositor->inputPanel()->setInputPanelVisible(textInput->inputPanelVisible());
}

void InputMethod::deactivate()
{
    if (!m_resource) {
        qDebug("Cannot deactivate (no input method running).");
        return;
    }

    send_deactivate(m_resource->handle, m_context->resource()->handle);
    m_textInput = 0;
    m_context = 0;

    m_compositor->inputPanel()->setFocus(0);
    m_compositor->inputPanel()->setCursorRectangle(QRect());
    m_compositor->inputPanel()->setInputPanelVisible(false);
}

void InputMethod::focusChanged(Surface *surface)
{
    if (!m_textInput)
        return;

    if (!surface || m_textInput->focus() != surface) {
        m_textInput->deactivate(this);
    }
}

bool InputMethod::isBound() const
{
    return m_resource != 0;
}

InputMethodContext *InputMethod::context() const
{
    return m_context;
}

TextInput *InputMethod::textInput() const
{
    return m_textInput;
}

void InputMethod::input_method_bind_resource(Resource *resource)
{
    if (m_resource) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "interface object already bound");
        wl_resource_destroy(resource->handle);
        return;
    }

    m_resource = resource;
}

void InputMethod::input_method_destroy_resource(Resource *resource)
{
    Q_ASSERT(resource == m_resource);
    m_resource = 0;
}

} // namespace QtWayland

QT_END_NAMESPACE
