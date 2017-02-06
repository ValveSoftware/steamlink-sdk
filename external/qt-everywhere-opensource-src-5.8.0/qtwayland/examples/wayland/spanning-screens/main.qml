/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.6
import QtQuick.Window 2.2
import QtWayland.Compositor 1.0

WaylandCompositor {
    id: wlcompositor

    WaylandOutput {
        compositor: wlcompositor
        sizeFollowsWindow: true
        window: Window {
            id: topSurfaceArea
            width: 1024
            height: 768
            visible: true
            color: "#1337af"
            Text { text: "Top screen" }
        }
    }

    WaylandOutput {
        compositor: wlcompositor
        sizeFollowsWindow: true
        window: Window {
            id: bottomSurfaceArea
            width: 1024
            height: 768
            visible: true
            color: "#1abacc"
            Text { text: "Bottom screen" }
        }
    }

    Component {
        id: chromeComponent
        WaylandQuickItem {
            onSurfaceDestroyed: destroy()
        }
    }

    Component {
        id: xdgPopupComponent
        ShellSurfaceItem {
            onSurfaceDestroyed: destroy()
        }
    }

    WlShell {
        onWlShellSurfaceCreated: handleShellSurfaceCreated(shellSurface)
    }

    property variant viewsBySurface: ({})

    XdgShellV5 {
        onXdgSurfaceCreated: handleShellSurfaceCreated(xdgSurface)
        onXdgPopupCreated: {
            var parentViews = viewsBySurface[xdgPopup.parentSurface];
            xdgPopupComponent.createObject(parentViews.top, { "shellSurface": xdgPopup } );
            xdgPopupComponent.createObject(parentViews.bottom, { "shellSurface": xdgPopup } );
        }
    }

    function handleShellSurfaceCreated(shellSurface) {
        var topItem = chromeComponent.createObject(topSurfaceArea, {
            "surface": shellSurface.surface
        });

        var bottomItem = chromeComponent.createObject(bottomSurfaceArea, {
            "surface": shellSurface.surface,
            "y": Qt.binding(function() { return -topSurfaceArea.height;})
        });

        viewsBySurface[shellSurface.surface] = {
            top: topItem,
            bottom: bottomItem
        };

        var height = bottomSurfaceArea.height + topSurfaceArea.height;
        var width = Math.max(bottomSurfaceArea.width, topSurfaceArea.width);
        var size = Qt.size(width, height);

        if (shellSurface.sendFullscreen) {
            shellSurface.sendFullscreen(size);
        } else if (shellSurface.sendConfigure) {
            shellSurface.sendConfigure(size, WlShellSurface.NoneEdge);
        }
    }
}
