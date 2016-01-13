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

#ifndef QTWAYLAND_QWLINPUTPANEL_P_H
#define QTWAYLAND_QWLINPUTPANEL_P_H

#include <QtCompositor/qwaylandexport.h>

#include <QtCompositor/private/qwayland-server-input-method.h>

#include <QRect>
#include <QScopedPointer>

QT_BEGIN_NAMESPACE

class QWaylandInputPanel;

namespace QtWayland {

class Compositor;
class Surface;
class TextInput;

class Q_COMPOSITOR_EXPORT InputPanel : public QtWaylandServer::wl_input_panel
{
public:
    InputPanel(Compositor *compositor);
    ~InputPanel();

    QWaylandInputPanel *handle() const;

    Surface *focus() const;
    void setFocus(Surface *focus);

    bool inputPanelVisible() const;
    void setInputPanelVisible(bool inputPanelVisible);

    QRect cursorRectangle() const;
    void setCursorRectangle(const QRect &cursorRectangle);

protected:
    void input_panel_get_input_panel_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) Q_DECL_OVERRIDE;

private:
    Compositor *m_compositor;
    QScopedPointer<QWaylandInputPanel> m_handle;

    Surface *m_focus;
    bool m_inputPanelVisible;
    QRect m_cursorRectangle;
};

} // namespace QtWayland

QT_END_NAMESPACE

#endif // QTWAYLAND_QWLINPUTPANEL_P_H
