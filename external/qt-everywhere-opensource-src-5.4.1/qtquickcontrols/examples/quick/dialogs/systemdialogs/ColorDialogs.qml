/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
import QtQuick.Controls 1.2
import QtQuick.Dialogs 1.1

Item {
    width: 320
    height: 240
    SystemPalette { id: palette }
    clip: true

    //! [colordialog]
    ColorDialog {
        id: colorDialog
        visible: colorDialogVisible.checked
        modality: colorDialogModal.checked ? Qt.WindowModal : Qt.NonModal
        title: "Choose a color"
        color: "green"
        showAlphaChannel: colorDialogAlpha.checked
        onAccepted: { console.log("Accepted: " + color) }
        onRejected: { console.log("Rejected") }
    }
    //! [colordialog]

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        Label {
            font.bold: true
            text: "Color dialog properties:"
        }
        CheckBox {
            id: colorDialogModal
            text: "Modal"
            checked: true
            Binding on checked { value: colorDialog.modality != Qt.NonModal }
        }
        CheckBox {
            id: colorDialogAlpha
            text: "Show alpha channel"
            Binding on checked { value: colorDialog.showAlphaChannel }
        }
        CheckBox {
            id: colorDialogVisible
            text: "Visible"
            Binding on checked { value: colorDialog.visible }
        }
        Row {
            id: colorRow
            spacing: parent.spacing
            height: colorLabel.implicitHeight * 2.0
            Rectangle {
                color: colorDialog.color
                height: parent.height
                width: height * 2
                border.color: "black"
                MouseArea {
                    anchors.fill: parent
                    onClicked: colorDialog.open()
                }
            }
            Label {
                id: colorLabel
                text: "<b>current color:</b> " + colorDialog.color
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: buttonRow.height * 1.2
        color: Qt.darker(palette.window, 1.1)
        border.color: Qt.darker(palette.window, 1.3)
        Row {
            id: buttonRow
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            height: implicitHeight
            width: parent.width
            Button {
                text: "Open"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: colorDialog.open()
            }
            Button {
                text: "Close"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: colorDialog.close()
            }
            Button {
                text: "set to green"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: colorDialog.color = "green"
            }
        }
    }
}
