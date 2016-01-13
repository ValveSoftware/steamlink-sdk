/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

import QtQuick 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2

ScrollView {
    id: page

    horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff

    Item {
        id: content

        width: Math.max(page.viewport.width, grid.implicitWidth + 2 * grid.rowSpacing)
        height: Math.max(page.viewport.height, grid.implicitHeight + 2 * grid.columnSpacing)

        GridLayout {
            id: grid

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: grid.rowSpacing
            anchors.rightMargin: grid.rowSpacing
            anchors.topMargin: grid.columnSpacing

            columns: page.width < page.height ? 1 : 2

            GroupBox {
                title: "Button"
                Layout.fillWidth: true
                Layout.columnSpan: grid.columns
                RowLayout {
                    anchors.fill: parent
                    Button { text: "OK"; isDefault: true }
                    Button { text: "Cancel" }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Attach"
                        menu: Menu {
                            MenuItem { text: "Image" }
                            MenuItem { text: "Document" }
                        }
                    }
                }
            }

            GroupBox {
                title: "CheckBox"
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    CheckBox { text: "E-mail"; checked: true }
                    CheckBox { text: "Calendar"; checked: true }
                    CheckBox { text: "Contacts" }
                }
            }

            GroupBox {
                title: "RadioButton"
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    ExclusiveGroup { id: radioGroup }
                    RadioButton { text: "Portrait"; exclusiveGroup: radioGroup }
                    RadioButton { text: "Landscape"; exclusiveGroup: radioGroup }
                    RadioButton { text: "Automatic"; exclusiveGroup: radioGroup; checked: true }
                }
            }

            GroupBox {
                title: "Switch"
                Layout.fillWidth: true
                Layout.columnSpan: grid.columns
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Label { text: "Wi-Fi"; Layout.fillWidth: true }
                        Switch { checked: true }
                    }
                    RowLayout {
                        Label { text: "Bluetooth"; Layout.fillWidth: true }
                        Switch { checked: false }
                    }
                }
            }
        }
    }
}
