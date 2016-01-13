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

#include "qwlinputpanel_p.h"

#include <QtCompositor/qwaylandinputpanel.h>

#include "qwlcompositor_p.h"
#include "qwlinputdevice_p.h"
#include "qwlinputmethod_p.h"
#include "qwlinputpanelsurface_p.h"
#include "qwlsurface_p.h"
#include "qwltextinput_p.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

InputPanel::InputPanel(Compositor *compositor)
    : QtWaylandServer::wl_input_panel(compositor->wl_display(), 1)
    , m_compositor(compositor)
    , m_handle(new QWaylandInputPanel(this))
    , m_focus()
    , m_inputPanelVisible(false)
    , m_cursorRectangle()
{
}

InputPanel::~InputPanel()
{
}

QWaylandInputPanel *InputPanel::handle() const
{
    return m_handle.data();
}

Surface *InputPanel::focus() const
{
    return m_focus;
}

void InputPanel::setFocus(Surface *focus)
{
    if (m_focus == focus)
        return;

    m_focus = focus;

    Q_EMIT handle()->focusChanged();
}

bool InputPanel::inputPanelVisible() const
{
    return m_inputPanelVisible;
}

void InputPanel::setInputPanelVisible(bool inputPanelVisible)
{
    if (m_inputPanelVisible == inputPanelVisible)
        return;

    m_inputPanelVisible = inputPanelVisible;

    Q_EMIT handle()->visibleChanged();
}

QRect InputPanel::cursorRectangle() const
{
    return m_cursorRectangle;
}

void InputPanel::setCursorRectangle(const QRect &cursorRectangle)
{
    if (m_cursorRectangle == cursorRectangle)
        return;

    m_cursorRectangle = cursorRectangle;

    Q_EMIT handle()->cursorRectangleChanged();
}

void InputPanel::input_panel_get_input_panel_surface(Resource *resource, uint32_t id, wl_resource *surface)
{
    new InputPanelSurface(resource->client(), id, Surface::fromResource(surface));
}

} // namespace QtWayland

QT_END_NAMESPACE
