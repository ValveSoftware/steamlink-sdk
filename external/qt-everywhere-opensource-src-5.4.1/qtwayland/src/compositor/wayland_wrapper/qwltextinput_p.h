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

#ifndef QTWAYLAND_QWLTEXTINPUT_P_H
#define QTWAYLAND_QWLTEXTINPUT_P_H

#include <QtCompositor/private/qwayland-server-text.h>

#include <QRect>

QT_BEGIN_NAMESPACE

namespace QtWayland {

class Compositor;
class InputMethod;
class Surface;

class TextInput : public QtWaylandServer::wl_text_input
{
public:
    explicit TextInput(Compositor *compositor, struct ::wl_client *client, int id);

    Surface *focus() const;

    bool inputPanelVisible() const;
    QRect cursorRectangle() const;

    void deactivate(InputMethod *inputMethod);

protected:
    void text_input_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;

    void text_input_activate(Resource *resource, wl_resource *seat, wl_resource *surface) Q_DECL_OVERRIDE;
    void text_input_deactivate(Resource *resource, wl_resource *seat) Q_DECL_OVERRIDE;
    void text_input_show_input_panel(Resource *resource) Q_DECL_OVERRIDE;
    void text_input_hide_input_panel(Resource *resource) Q_DECL_OVERRIDE;
    void text_input_reset(Resource *resource) Q_DECL_OVERRIDE;
    void text_input_commit_state(Resource *resource, uint32_t serial) Q_DECL_OVERRIDE;
    void text_input_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose) Q_DECL_OVERRIDE;
    void text_input_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) Q_DECL_OVERRIDE;
    void text_input_set_preferred_language(Resource *resource, const QString &language) Q_DECL_OVERRIDE;
    void text_input_set_surrounding_text(Resource *resource, const QString &text, uint32_t cursor, uint32_t anchor) Q_DECL_OVERRIDE;
    void text_input_invoke_action(Resource *resource, uint32_t button, uint32_t index) Q_DECL_OVERRIDE;

private:
    Compositor *m_compositor;
    QList<InputMethod*> m_activeInputMethods;
    Surface *m_focus;

    bool m_inputPanelVisible;
    QRect m_cursorRectangle;

};

} // namespace QtWayland

QT_END_NAMESPACE

#endif // QTWAYLAND_QWLTEXTINPUT_P_H
