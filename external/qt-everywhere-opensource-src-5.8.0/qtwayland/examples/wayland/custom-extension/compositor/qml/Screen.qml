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
import QtQuick.Window 2.2
import QtWayland.Compositor 1.0

WaylandOutput {
    id: output
    property alias surfaceArea: background
    sizeFollowsWindow: true
    window: Window {
        id: screen

        property QtObject output

        width: 1600
        height: 900
        visible: true

        Rectangle {
            id: sidebar
            width: 150
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: "lightgray"
            Column {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 5

                Repeater {
                    model: comp.itemList
                    Rectangle {
                        height: 36
                        width: sidebar.width - 5
                        color: "white"
                        radius: 5
                        Text {
                            text: "window: " + modelData.shellSurface.title + "[" + modelData.shellSurface.className
                                  + (modelData.isCustom ? "]\nfont size: " + modelData.fontSize :"]\n No extension")
                            color: modelData.isCustom ? "black" : "darkgray"
                        }
                        MouseArea {
                            enabled: modelData.isCustom
                            anchors.fill: parent
                            onWheel: {
                                if (wheel.angleDelta.y > 0)
                                    modelData.fontSize++
                                else if (wheel.angleDelta.y < 0 && modelData.fontSize > 3)
                                    modelData.fontSize--
                            }
                            onDoubleClicked: {
                                output.compositor.customExtension.close(modelData.surface)
                            }
                        }
                    }
                }
                Text {
                    visible: comp.itemList.length > 0
                    width: sidebar.width - 5
                    text: "Mouse wheel to change font size. Double click to close"
                    wrapMode: Text.Wrap
                }
            }
        }

        WaylandMouseTracker {
            id: mouseTracker
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            windowSystemCursorEnabled: true
            Image {
                id: background
                anchors.fill: parent
                fillMode: Image.Tile
                source: "qrc:/images/background.png"
                smooth: false
            }
            WaylandCursorItem {
                id: cursor
                inputEventsEnabled: false
                x: mouseTracker.mouseX
                y: mouseTracker.mouseY

                seat: output.compositor.defaultSeat
            }

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                width: 100
                height: 100
                property bool on : true
                color: on ? "#DEC0DE" : "#FACADE"
                Text {
                    anchors.fill: parent
                    text: "Toggle window decorations"
                    wrapMode: Text.WordWrap
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        parent.on = !parent.on
                        comp.setDecorations(parent.on);
                    }
                }
            }
        }
    }
}
