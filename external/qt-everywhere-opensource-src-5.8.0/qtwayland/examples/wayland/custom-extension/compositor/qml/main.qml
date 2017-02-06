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

import QtQuick 2.0
import QtWayland.Compositor 1.0

import com.theqtcompany.customextension 1.0

WaylandCompositor {
    id: comp

    property alias customExtension: custom
    property var itemList: []

    function itemForSurface(surface) {
        var n = itemList.length
        for (var i = 0; i < n; i++) {
            if (itemList[i].surface === surface)
                return itemList[i]
        }
    }

    Screen {
        compositor: comp
    }

    Component {
        id: chromeComponent
        ShellSurfaceItem {
            id: chrome

            property bool isCustom
            property int fontSize: 12

            onSurfaceDestroyed: {
                var index = itemList.indexOf(chrome);
                if (index > -1) {
                    var listCopy = itemList
                    listCopy.splice(index, 1);
                    itemList = listCopy
                }
                chrome.destroy()
            }
            transform: [
                Rotation {
                    id: xRot
                    origin.x: chrome.width/2; origin.y: chrome.height/2;
                    angle: 0
                    axis { x: 1; y: 0; z: 0 }
                },
                Rotation {
                    id: yRot
                    origin.x: chrome.width/2; origin.y: chrome.height/2;
                    angle: 0
                    axis { x: 0; y: 1; z: 0 }
                }
            ]
            NumberAnimation {
                id: spinAnimation
                running: false
                loops: 2
                target: yRot;
                property: "angle";
                from: 0; to: 360;
                duration: 400;
            }

            function doSpin(ms) {
                console.log("spin " + ms)
                // using the 'ms' argument is left as an exercise for the reader...
                spinAnimation.start()
            }

            NumberAnimation {
                id: bounceAnimation
                running: false
                target: chrome
                property: "y"
                from: 0
                to: output.window.height - chrome.height
                easing.type: Easing.OutBounce
                duration: 1000
            }
            function doBounce(ms) {
                console.log("bounce " + ms)
                // using the 'ms' argument is left as an exercise for the reader...
                bounceAnimation.start()
            }
            onFontSizeChanged: {
                custom.setFontSize(surface, fontSize)
            }
        }
    }

    WlShell {
        id: defaultShell
        onWlShellSurfaceCreated: {
            var item = chromeComponent.createObject(defaultOutput.surfaceArea, { "shellSurface": shellSurface } );
            var w = defaultOutput.surfaceArea.width/2
            var h = defaultOutput.surfaceArea.height/2
            item.x = Math.random()*w
            item.y = Math.random()*h
            var listCopy = itemList // List properties cannot be modified through Javascript operations
            listCopy.push(item)
            itemList = listCopy
        }
    }

    CustomExtension {
        id: custom

        onSurfaceAdded: {
            var item = itemForSurface(surface)
            item.isCustom = true
        }
        onBounce: {
            var item = itemForSurface(surface)
            item.doBounce(ms)
        }
        onSpin: {
            var item = itemForSurface(surface)
            item.doSpin(ms)
        }
    }

    function setDecorations(shown) {
        var n = itemList.length
        for (var i = 0; i < n; i++) {
            // TODO: we only need to do it once for each client
            if (itemList[i].isCustom)
                custom.showDecorations(itemList[i].surface.client, shown)
        }
    }
}
