/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef WLEXTENDEDOUTPUT_H
#define WLEXTENDEDOUTPUT_H

#include "wayland-server.h"

#include <QtCompositor/qwaylandexport.h>

#include <QtCore/qnamespace.h>

#include <QtCompositor/private/qwayland-server-output-extension.h>

QT_BEGIN_NAMESPACE

namespace QtWayland {

class Compositor;
class Output;

class ExtendedOutput : public QtWaylandServer::qt_extended_output::Resource
{
public:
    ExtendedOutput() : output(0) {}

    Output *output;
};

class OutputExtensionGlobal : public QtWaylandServer::qt_output_extension, public QtWaylandServer::qt_extended_output
{
public:
    OutputExtensionGlobal(Compositor *compositor);

private:
    Compositor *m_compositor;

    qt_extended_output::Resource *extended_output_allocate() Q_DECL_OVERRIDE { return new ExtendedOutput; }

    void output_extension_get_extended_output(qt_output_extension::Resource *resource,
                                              uint32_t id,
                                              struct wl_resource *output_resource) Q_DECL_OVERRIDE;
};


}

QT_END_NAMESPACE

#endif // WLEXTENDEDOUTPUT_H
