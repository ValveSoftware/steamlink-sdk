/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Wayland module
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

#ifndef CUSTOMEXTENSION_H
#define CUSTOMEXTENSION_H

#include "wayland-util.h"

#include <QtWaylandCompositor/QWaylandCompositorExtensionTemplate>
#include <QtWaylandCompositor/QWaylandQuickExtension>
#include <QtWaylandCompositor/QWaylandCompositor>
#include "qwayland-server-custom.h"

class CustomExtension  : public QWaylandCompositorExtensionTemplate<CustomExtension>
        , public QtWaylandServer::qt_example_extension
{
    Q_OBJECT
public:
    CustomExtension(QWaylandCompositor *compositor = 0);
    void initialize() Q_DECL_OVERRIDE;

signals:
    void surfaceAdded(QWaylandSurface *surface);
    void bounce(QWaylandSurface *surface, uint ms);
    void spin(QWaylandSurface *surface, uint ms);

public slots:
    void setFontSize(QWaylandSurface *surface, uint pixelSize);
    void showDecorations(QWaylandClient *client, bool);
    void close(QWaylandSurface *surface);

protected:
    void example_extension_bounce(Resource *resource, wl_resource *surface, uint32_t duration) Q_DECL_OVERRIDE;
    void example_extension_spin(Resource *resource, wl_resource *surface, uint32_t duration) Q_DECL_OVERRIDE;
    void example_extension_register_surface(Resource *resource, wl_resource *surface) Q_DECL_OVERRIDE;
};

Q_COMPOSITOR_DECLARE_QUICK_EXTENSION_CLASS(CustomExtension)

#endif // CUSTOMEXTENSION_H
