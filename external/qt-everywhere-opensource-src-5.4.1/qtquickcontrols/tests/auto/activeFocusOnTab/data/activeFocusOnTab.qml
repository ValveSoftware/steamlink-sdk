/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
        Button {
            id: button1
            objectName: "button1"
            text: "button 1"
        }
        Button {
            id: button2
            objectName: "button2"
            text: "button 2"
        }
        Label {
            id: label
            objectName: "label"
            text: "label"
        }
        ToolButton {
            id: toolbutton
            objectName: "toolbutton"
            iconSource: "images/testIcon.png"
            tooltip: "Test Icon"
        }
        ListModel {
            id: choices
            ListElement { text: "Banana" }
            ListElement { text: "Orange" }
            ListElement { text: "Apple" }
            ListElement { text: "Coconut" }
        }
        ComboBox {
            id: combobox;
            objectName: "combobox"
            model: choices;
        }
        ComboBox {
            id: editable_combobox;
            objectName: "editable_combobox"
            model: choices;
            editable: true;
        }
        GroupBox {
            id: group1
            objectName: "group1"
            title: "GroupBox 1"
            checkable: true
            __checkbox.objectName: "group1_checkbox"
            Row {
                CheckBox {
                    id: checkbox1
                    objectName: "checkbox1"
                    text: "Text frame"
                    checked: true
                }
                CheckBox {
                    id: checkbox2
                    objectName: "checkbox2"
                    text: "Tickmarks"
                    checked: false
                }
            }
        }
        GroupBox {
            id: group2
            objectName: "group2"
            title: "GroupBox 2"
            Row {
                RadioButton {
                    id: radiobutton1
                    objectName: "radiobutton1"
                    text: "North"
                    checked: true
                }
                RadioButton {
                    id: radiobutton2
                    objectName: "radiobutton2"
                    text: "South"
                }
            }
        }
        //Page
        //ProgressBar maybe not need
        ProgressBar {
            id: progressbar
            objectName: "progressbar"
            indeterminate: true
        }
        //ScrollArea
        Slider {
            id: slider
            objectName: "slider"
            value: 0.5
        }
        SpinBox {
            id: spinbox
            objectName: "spinbox"
            width: 70
            minimumValue: 0
            maximumValue: 100
            value: 50
        }
        //SplitterColumn and SplitterRow false
        //StatusBar false
        TabView {
            id: tabview
            objectName: "tabview"
            width: 200
            Tab {
                id: tab1
                objectName: "tab1"
                title: "Tab1"
                Column {
                    id: column_in_tab1
                    objectName: "column_in_tab1"
                    anchors.fill: parent
                    Button {
                        id: tab1_btn1
                        objectName: "tab1_btn1"
                        text: "button 1 in Tab1"
                    }
                    Button {
                        id: tab1_btn2
                        objectName: "tab1_btn2"
                        text: "button 2 in Tab1"
                    }
                }
            }
            Tab {
                id: tab2
                objectName: "tab2"
                title: "Tab2"
                Column {
                    id: column_in_tab2
                    objectName: "column_in_tab2"
                    anchors.fill: parent
                    Button {
                        id: tab2_btn1
                        objectName: "tab2_btn1"
                        text: "button 1 in Tab2"
                    }
                    Button {
                        id: tab2_btn2
                        objectName: "tab2_btn2"
                        text: "button 2 in Tab2"
                    }
                }
            }
        }
        TextField {
            id: textfield
            objectName: "textfield"
            text: "abc"
        }
        TableView {
            id: tableview
            objectName: "tableview"
        }
        TextArea {
            id: textarea
            objectName: "textarea"
            text: "abc"
        }
    }
}
