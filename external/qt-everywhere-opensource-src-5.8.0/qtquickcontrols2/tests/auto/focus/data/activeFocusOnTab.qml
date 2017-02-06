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

import QtQuick 2.5
import QtQuick.Controls 2.1

Item {
    id: main
    objectName: "main"
    width: 400
    height: 800
    focus: true
    Component.onCompleted: button1.focus = true
    Column {
        anchors.fill: parent
        id: column
        objectName: "column"
        BusyIndicator {
            id: busyindicator
            objectName: "busyindicator"
        }
        Button {
            id: button1
            objectName: "button1"
            text: "button1"
        }
        Button {
            id: button2
            objectName: "button2"
            text: "button2"
        }
        CheckBox {
            id: checkbox
            objectName: "checkbox"
            text: "checkbox"
        }
        GroupBox {
            id: groupbox1
            objectName: "groupbox1"
            title: "grouppox1"
            Column {
                anchors.fill: parent
                CheckBox {
                    id: checkbox1
                    objectName: "checkbox1"
                    text: "checkbox1"
                }
                CheckBox {
                    id: checkbox2
                    objectName: "checkbox2"
                    text: "checkbox2"
                }
            }
        }
        Label {
            id: label
            objectName: "label"
            text: "label"
        }
        PageIndicator {
            id: pageindicator
            objectName: "pageindicator"
        }
        ProgressBar {
            id: progressbar
            objectName: "progressbar"
            indeterminate: true
        }
        RadioButton {
            id: radiobutton
            objectName: "radiobutton"
            text: "radiobutton"
        }
        GroupBox {
            id: groupbox2
            objectName: "groupbox2"
            title: "groupbox2"
            Column {
                anchors.fill: parent
                RadioButton {
                    id: radiobutton1
                    objectName: "radiobutton1"
                    text: "radiobutton1"
                }
                RadioButton {
                    id: radiobutton2
                    objectName: "radiobutton2"
                    text: "radiobutton2"
                }
            }
        }
        RangeSlider {
            id: rangeslider
            objectName: "rangeslider"
            first.handle.objectName: "rangeslider.first"
            second.handle.objectName: "rangeslider.second"
        }
        // ScrollBar
        ScrollIndicator {
            id: scrollindicator
            objectName: "scrollindicator"
        }
        Slider {
            id: slider
            objectName: "slider"
            value: 0.5
        }
        SpinBox {
            id: spinbox
            objectName: "spinbox"
            editable: true
            value: 50
        }
        // StackView
        Switch {
            id: swtich // switch
            objectName: "switch"
            text: "switch"
        }
        TabBar {
            width: parent.width
            id: tabbar
            objectName: "tabbar"
            TabButton {
                id: tabbutton1
                objectName: "tabbutton1"
                text: "tabbutton1"
            }
            TabButton {
                id: tabbutton2
                objectName: "tabbutton2"
                text: "tabbutton2"
            }
        }
        TextField {
            id: textfield
            objectName: "textfield"
            text: "abc"
        }
        ToolBar {
            width: parent.width
            id: toolbar
            objectName: "toolbar"
            ToolButton {
                id: toolbutton
                objectName: "toolbutton"
                text: "toolbutton"
            }
        }
        TextArea {
            id: textarea
            objectName: "textarea"
            text: "abc"
        }
    }
}
