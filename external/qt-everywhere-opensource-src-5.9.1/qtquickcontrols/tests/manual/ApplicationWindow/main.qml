/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1


ApplicationWindow {
    id: window
    title: "Window constraints"

    property var g_content: canvas_Component.createObject(contentItem)

    menuBar: MenuBar {
        Menu {
            title: "Options"
            MenuItem {
                id: ckToolBarVisible
                text: "Show ToolBar"
                checkable: true
                checked: true
            }
            MenuItem {
                id: ckStatusBarVisible
                text: "Show StatusBar"
                checkable: true
                checked: true
            }
            MenuItem {
                id: ckUseLayout
                text: "Use layout"
                checkable: true
                checked: false
                onCheckedChanged: {
                    if (g_content) {
                        g_content.destroy()
                    }
                    if (checked) {
                        g_content = layout_Component.createObject(contentItem)
                    } else {
                        g_content = canvas_Component.createObject(contentItem)
                    }
                }
            }
        }
    }


    toolBar: ToolBar {
        visible: ckToolBarVisible.checked
        Row {
            ToolButton {
                text: "One"
            }
            ToolButton {
                text: "Two"
            }
        }
    }

    statusBar: StatusBar {
        visible: ckStatusBarVisible.checked
        Row {
            Label {
                text: "Window size:(" + window.width + "x" + window.height + ")"
            }
        }
    }

    Component {
        id: layout_Component
        GridLayout {
            id: layout
            anchors.fill: parent
            rowSpacing: 2
            columnSpacing: 2
            columns: 5
            Repeater {
                model: 10
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: 'red'
                    Layout.minimumWidth: 30
                    Layout.minimumHeight: 30
                    implicitWidth: 42
                    implicitHeight: 42
                    Layout.maximumWidth: 200
                    Layout.maximumHeight: 200
                    Text {
                        anchors.centerIn: parent
                        text: parent.width + "x" + parent.height
                    }
                }
            }
        }
    }


    Component {
        id: canvas_Component
        Canvas {
            id: canvas
            width: 200
            height: 200
            antialiasing: false

            onPaint: {
                var ctx = getContext('2d')
                if (ctx) {
                    ctx.save()
                    ctx.fillStyle = "#e0e0e0"
                    ctx.strokeStyle = "#000000"
                    ctx.lineWidth = 1


                    ctx.fillRect(0, 0, canvas.width, canvas.height)

                    ctx.fillStyle = "#000000"
                    ctx.strokeStyle = "#000000"
                    ctx.antiAliasing= false

                    ctx.beginPath();
                    for (var x = 5; x < Math.max(canvas.width, canvas.height); x+=5) {
                        var extent = 2
                        if (x % 100 == 0)
                            extent = 10
                        else if (x % 10 == 0)
                            extent = 5
                        else extent = 2
                        ctx.moveTo(x, 0)
                        ctx.lineTo(x, 0 + extent)
                        ctx.moveTo(0, x)
                        ctx.lineTo(0 + extent, x)
                    }
                    ctx.closePath()
                    ctx.stroke()

                    ctx.restore()
                }
            }
        }
    }
}
