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

import QtQuick 2.0
import "../components"

Item {
    id: dialog

    signal goButtonClicked
    signal cancelButtonClicked
    signal clearButtonClicked

    property alias title: titleBar.text

    property int gap: 10
    property bool showButtons: true
    property Item item

    opacity: 0
    anchors.fill: parent
    enabled: opacity > 0 ? true : false

    Fader {}

    onItemChanged: {
        if (item)
            item.parent = dataRect;
    }

    Rectangle {
        id: dialogRectangle

        property int maximumDialogHeight: {
            if (dialog.opacity === 0 ||
                (Qt.inputMethod.keyboardRectangle.width === 0 && Qt.inputMethod.keyboardRectangle.height === 0)) {
                return dialog.height;
            } else {
                return dialog.mapFromItem(null, Qt.inputMethod.keyboardRectangle.x, Qt.inputMethod.keyboardRectangle.y).y
            }
        }
        property int maximumContentHeight: maximumDialogHeight - titleBar.height - buttons.height - gap*1.5

        color: "#ECECEC"
        opacity: parent.opacity

        height: dataRect.height + titleBar.height + buttons.height + gap*1.5
        y: (maximumDialogHeight - height) / 2
        anchors.left: parent.left
        anchors.leftMargin: gap/2
        anchors.right: parent.right
        anchors.rightMargin: gap/2

        radius: 5

        TitleBar {
            id: titleBar;

            height: 40
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            opacity: 0.9
            onClicked: {
                Qt.inputMethod.hide();
                dialog.cancelButtonClicked();
            }

        }

        Rectangle {
            id: dataRect
            color: "#ECECEC"
            radius: 5

            anchors.top: titleBar.bottom
            anchors.left: dialogRectangle.left
            anchors.right: dialogRectangle.right
            anchors.margins: gap/2
            height: Math.min(dialogRectangle.maximumContentHeight, item ? item.implicitHeight : 0)

            Binding {
                target: item
                property: "anchors.fill"
                value: dataRect
            }
            Binding {
                target: item
                property: "anchors.margins"
                value: gap/2
            }
        }

        Row {
            id: buttons
            anchors.top: dataRect.bottom
            anchors.topMargin: gap/2
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: gap/3
            height: 32
            visible: showButtons
            Button {
                id: buttonClearAll
                text: "Clear"
                width: 80
                height: parent.height
                onClicked: dialog.clearButtonClicked()
            }
            Button {
                id: buttonGo
                text: "Go!"
                width: 80
                height: parent.height
                onClicked: dialog.goButtonClicked()
            }
        }
    }
}
