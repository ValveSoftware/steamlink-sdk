/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
import QtQuick.Controls 2.1
import QtQuick.Controls.Material 2.1
import QtQuick.Controls.Universal 2.1

ApplicationWindow {
    id: window
    visible: true
    title: "Buttons"

    Component.onCompleted: {
        var pane = repeater.itemAt(0)
        width = pane.implicitWidth * 2 + flickable.leftMargin + flickable.rightMargin + flow.spacing
        height = header.height + pane.implicitHeight * 2 + flickable.topMargin + flickable.bottomMargin + flow.spacing
    }

    header: ToolBar {
        Row {
            spacing: 20
            anchors.right: parent.right
            CheckBox {
                id: hoverBox
                text: "Hover"
                checked: true
            }
            CheckBox {
                id: roundBox
                text: "Round"
                checked: false
            }
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent

        topMargin: 40
        leftMargin: 40
        rightMargin: 40
        bottomMargin: 40

        contentHeight: flow.implicitHeight

        Flow {
            id: flow
            spacing: 40
            width: flickable.width - flickable.leftMargin - flickable.rightMargin

            Repeater {
                id: repeater

                model: [
                    { title: "Normal", theme: Material.Light, flat: false },
                    { title: "Flat", theme: Material.Light, flat: true },
                    { title: "Normal", theme: Material.Dark, flat: false },
                    { title: "Flat", theme: Material.Dark, flat: true }
                ]

                Pane {
                    Material.elevation: 8
                    Material.theme: modelData.theme
                    Universal.theme: modelData.theme

                    GroupBox {
                        title: modelData.title
                        background.visible: false

                        Grid {
                            columns: 4
                            spacing: 20
                            padding: 20

                            ButtonLoader { text: "Normal";   flat: modelData.flat; hoverEnabled: hoverBox.checked; round: roundBox.checked }
                            ButtonLoader { text: "Disabled"; flat: modelData.flat; hoverEnabled: hoverBox.checked; enabled: false; round: roundBox.checked }
                            ButtonLoader { text: "Down";     flat: modelData.flat; hoverEnabled: hoverBox.checked; down: true; round: roundBox.checked }
                            ButtonLoader { text: "Disabled"; flat: modelData.flat; hoverEnabled: hoverBox.checked; down: true; enabled: false; round: roundBox.checked }

                            ButtonLoader { text: "Checked";  flat: modelData.flat; hoverEnabled: hoverBox.checked; checked: true; round: roundBox.checked }
                            ButtonLoader { text: "Disabled"; flat: modelData.flat; hoverEnabled: hoverBox.checked; checked: true; enabled: false; round: roundBox.checked }
                            ButtonLoader { text: "Down";     flat: modelData.flat; hoverEnabled: hoverBox.checked; checked: true; down: true; round: roundBox.checked }
                            ButtonLoader { text: "Disabled"; flat: modelData.flat; hoverEnabled: hoverBox.checked; checked: true; down: true; enabled: false; round: roundBox.checked }

                            ButtonLoader { text: "Highlighted"; flat: modelData.flat; hoverEnabled: hoverBox.checked; highlighted: true; round: roundBox.checked }
                            ButtonLoader { text: "Disabled";    flat: modelData.flat; hoverEnabled: hoverBox.checked; highlighted: true; enabled: false; round: roundBox.checked }
                            ButtonLoader { text: "Down";        flat: modelData.flat; hoverEnabled: hoverBox.checked; highlighted: true; down: true; round: roundBox.checked }
                            ButtonLoader { text: "Disabled";    flat: modelData.flat; hoverEnabled: hoverBox.checked; highlighted: true; down: true; enabled: false; round: roundBox.checked }

                            ButtonLoader { text: "Hi-checked"; flat: modelData.flat; hoverEnabled: hoverBox.checked; highlighted: true; checked: true; round: roundBox.checked }
                            ButtonLoader { text: "Disabled";   flat: modelData.flat; hoverEnabled: hoverBox.checked; highlighted: true; checked: true; enabled: false; round: roundBox.checked }
                            ButtonLoader { text: "Down";       flat: modelData.flat; hoverEnabled: hoverBox.checked; highlighted: true; checked: true; down: true; round: roundBox.checked }
                            ButtonLoader { text: "Disabled";   flat: modelData.flat; hoverEnabled: hoverBox.checked; highlighted: true; checked: true; down: true; enabled: false; round: roundBox.checked }
                        }
                    }
                }
            }
        }

        ScrollIndicator.vertical: ScrollIndicator { }
    }
}
