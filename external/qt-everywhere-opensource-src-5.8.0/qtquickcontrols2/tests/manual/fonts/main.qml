/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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
import QtQuick.Templates 2.1 as T
import QtQuick.Layouts 1.2

ApplicationWindow {
    visible: true
    width: 480
    height: 640
    title: qsTr("Hello World")

    header: ToolBar {
        Slider {
            from: 16
            to: 48
            stepSize: 1
            onValueChanged: control.font.pointSize = value
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: control.width
        contentHeight: control.height

        T.Control {
            id: control
            width: layout.implicitWidth + 40
            height: layout.implicitHeight + 40
            ColumnLayout {
                id: layout
                anchors.fill: parent
                anchors.margins: 20
                Button { text: "Button" }
                CheckBox { text: "CheckBox" }
                GroupBox { title: "GroupBox" }
                RadioButton { text: "RadioButton" }
                Switch { text: "Switch" }
                TabButton {
                    text: "TabButton"
                    font.pointSize: control.font.pointSize
                }
                TextField { placeholderText: "TextField" }
                TextArea { placeholderText: "TextArea" }
                ToolButton { text: "ToolButton" }
                Tumbler { model: 3 }
            }
        }

        ScrollBar.vertical: ScrollBar { }
    }
}
