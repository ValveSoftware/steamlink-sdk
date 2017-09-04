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
import QtQuick.Controls.Styles 1.1
import QtQuick.Controls.Private 1.0

Item {
    property Component button: Button { text: "Push me"}
    property Component calendar: Calendar {}
    property Component checkbox: CheckBox { text: "A CheckBox" }
    property Component toolbutton: ToolButton { text: "A ToolButton" }
    property Component radiobutton: RadioButton { text: "A RadioButton" }
    property Component textfield: TextField { }
    property Component busyIndicator: BusyIndicator { }
    property Component spinbox: SpinBox {}
    property Component slider : Slider {}
    property Component combobox: ComboBox { model: testDataModel }
    property Component textarea: TextArea { text: loremIpsum }
    property Component toolbar: ToolBar { }
    property Component statusbar: StatusBar { }
    property Component switchcontrol: Switch { }
    property Component label: Label {text: "I am a label" }
    property Component tableview: TableView { property bool movableColumns: true; model: testDataModel ; TableViewColumn {title: "Column 1"; movable: movableColumns} TableViewColumn {title: "Column 2"; movable: movableColumns} }
    property Component tabView: TabView { Repeater { model: 3 ; delegate:Tab { title: "Tab " + index } }}
    property Component scrollview: ScrollView {
        Rectangle {
            color: "#eee"
            width: 1000
            height: 1000
            Image {
                source: "../../images/checkered.png" ;
                fillMode: Image.Tile
                anchors.fill: parent
                opacity: 0.1
            }
        }
    }

    property Component groupbox:  GroupBox {
        Column {
            CheckBox {
                text: "checkbox1"
            }
            CheckBox {
                text: "checkbox2"
            }
        }
    }

    property Component progressbar: ProgressBar {
        property bool ___isRunning: true
        Timer {
            id: timer
            running: ___isRunning
            repeat: true
            interval: 25
            onTriggered: {
                var next = parent.value + 0.01;
                parent.value = (next > parent.maximumValue) ? parent.minimumValue : next;
            }
        }
    }

    property var model: ListModel{
        id: testDataModel
        Component.onCompleted: {
            for (var i = 0 ; i < 10 ; ++i)
                testDataModel.append({text: "Value " + i});
        }
    }

    property string loremIpsum:
        "<a href='lipsum.com'>Lorem ipsum</a> dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor "+
        "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor "+
        "incididunt ut labore et dolore magna aliqua.\n Ut enim ad minim veniam, quis nostrud "+
        "exercitation ullamco laboris nisi ut aliquip ex ea commodo cosnsequat. ";

    property var componentModel: ListModel {
        Component.onCompleted: {
            append({ name: "Button",        component: button});
            append({ name: "Calendar",      component: calendar});
            append({ name: "BusyIndicator", component: busyIndicator});
            append({ name: "ToolButton",    component: toolbutton});
            append({ name: "CheckBox",      component: checkbox});
            append({ name: "ComboBox",      component: combobox});
            append({ name: "RadioButton",   component: radiobutton});
            append({ name: "Slider",        component: slider});
            append({ name: "Switch",        component: switchcontrol});
            append({ name: "ProgressBar",   component: progressbar});
            append({ name: "TextField",     component: textfield});
            append({ name: "TextArea",      component: textarea});
            append({ name: "SpinBox",       component: spinbox});
            append({ name: "ToolBar",       component: toolbar});
            append({ name: "StatusBar",     component: statusbar});
            append({ name: "TableView",     component: tableview});
            append({ name: "ScrollView",    component: scrollview});
            append({ name: "GroupBox",      component: groupbox});
            append({ name: "TabView",       component: tabView});
            append({ name: "Label",         component: label});
        }
    }

    property Component buttonStyle: ButtonStyle {}
    property Component calendarStyle: CalendarStyle {}
    property Component toolbuttonStyle: ToolButtonStyle {}
    property Component checkboxStyle: CheckBoxStyle {}
    property Component comboboxStyle: ComboBoxStyle {}
    property Component radiobuttonStyle: RadioButtonStyle {}
    property Component sliderStyle: SliderStyle {}
    property Component progressbarStyle: ProgressBarStyle {}
    property Component textfieldStyle: TextFieldStyle {}
    property Component textareaStyle: TextAreaStyle {}
    property Component spinboxStyle: SpinBoxStyle {}
    property Component toolbarStyle: ToolBarStyle {}
    property Component statusbarStyle: StatusBarStyle {}
    property Component switchStyle: SwitchStyle {}
    property Component tableviewStyle: TableViewStyle {}
    property Component scrollviewStyle: ScrollViewStyle {}
    property Component groupboxStyle: GroupBoxStyle {}
    property Component tabViewStyle: TabViewStyle {}
    property Component busyIndicatorStyle: BusyIndicatorStyle {}
    property Component labelStyle: null

    property var customStyles: ListModel {
        Component.onCompleted: {
            append({ name: "Button",        component: buttonStyle});
            append({ name: "BusyIndicator", component: busyIndicatorStyle});
            append({ name: "Calendar",      component: calendarStyle});
            append({ name: "ToolButton",    component: toolbuttonStyle});
            append({ name: "CheckBox",      component: checkboxStyle});
            append({ name: "ComboBox",      component: comboboxStyle});
            append({ name: "RadioButton",   component: radiobuttonStyle});
            append({ name: "Slider",        component: sliderStyle});
            append({ name: "Switch",        component: switchStyle});
            append({ name: "ProgressBar",   component: progressbarStyle});
            append({ name: "TextField",     component: textfieldStyle});
            append({ name: "TextArea",      component: textareaStyle});
            append({ name: "SpinBox",       component: spinboxStyle});
            append({ name: "ToolBar",       component: toolbarStyle});
            append({ name: "StatusBar",     component: statusbarStyle});
            append({ name: "TableView",     component: tableviewStyle});
            append({ name: "ScrollView",    component: scrollviewStyle});
            append({ name: "GroupBox",      component: groupboxStyle});
            append({ name: "TabView",       component: tabViewStyle});
            append({ name: "Label",         component: labelStyle});
        }
    }
}
