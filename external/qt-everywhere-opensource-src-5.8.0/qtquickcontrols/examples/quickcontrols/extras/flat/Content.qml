/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.4
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles.Flat 1.0 as Flat
import QtQuick.Extras 1.4
import QtQuick.XmlListModel 2.0

Item {
    anchors.fill: parent

    Text {
        id: text
        visible: false
    }

    FontMetrics {
        id: fontMetrics
        font.family: Flat.FlatStyle.fontFamily
    }

    readonly property int layoutSpacing: Math.round(5 * Flat.FlatStyle.scaleFactor)

    property var componentModel: [
        { name: "Buttons", component: buttonsComponent },
        { name: "Calendar", component: calendarComponent },
        { name: "DelayButton", component: delayButtonComponent },
        { name: "Dial", component: dialComponent },
        { name: "Input", component: inputComponent },
        { name: "PieMenu", component: pieMenuComponent },
        { name: "Progress", component: progressComponent },
        { name: "TableView", component: tableViewComponent },
        { name: "TextArea", component: textAreaComponent },
        { name: "Tumbler", component: tumblerComponent }
    ]

    Loader {
        id: componentLoader
        anchors.fill: parent
        sourceComponent: componentModel[controlData.componentIndex].component
    }

    property Component buttonsComponent: ScrollView {
        id: scrollView
        horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff

        Flickable {
            anchors.fill: parent
            contentWidth: viewport.width
            contentHeight: buttoncolumn.implicitHeight + textMargins * 1.5
            ColumnLayout {
                id: buttoncolumn
                anchors.fill: parent
                anchors.margins: textMargins
                anchors.topMargin: textMargins / 2
                spacing: textMargins / 2
                enabled: !settingsData.allDisabled

                GroupBox {
                    title: "Button"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    GridLayout {
                        columns: Math.max(1, Math.floor(scrollView.width / button.implicitWidth - 0.5))
                        Button {
                            id: button
                            text: "Normal"
                        }
                        Button {
                            text: "Default"
                            isDefault: true
                        }
                        Button {
                            text: "Checkable"
                            checkable: true
                        }
                        Button {
                            text: "Menu"
                            menu: Menu {
                                MenuItem { text: "Normal"; shortcut: "Ctrl+N" }
                                MenuSeparator { }
                                MenuItem { text: "Checkable 1"; checkable: true; checked: true }
                                MenuItem { text: "Checkable 2"; checkable: true; checked: true }
                                MenuSeparator { }
                            }
                            visible: Qt.application.supportsMultipleWindows
                        }
                    }
                }

                GroupBox {
                    title: "RadioButton"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    ExclusiveGroup { id: radiobuttongroup }
                    ColumnLayout {
                        anchors.fill: parent
                        Repeater {
                            model: ["First", "Second", "Third"]
                            RadioButton {
                                text: modelData
                                checked: index === 0
                                exclusiveGroup: radiobuttongroup
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                GroupBox {
                    title: "CheckBox"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        Repeater {
                            model: ["First", "Second", "Third"]
                            CheckBox {
                                text: modelData
                                checked: index === 0
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                GroupBox {
                    title: "Switch"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        Repeater {
                            model: ["First", "Second", "Third"]
                            RowLayout {
                                spacing: layoutSpacing * 2
                                Label {
                                    text: modelData
                                    font.family: Flat.FlatStyle.fontFamily
                                    renderType: Text.QtRendering
                                    Layout.preferredWidth: fontMetrics.advanceWidth("Second")
                                }
                                Switch { checked: index == 0 }
                            }
                        }
                    }
                }

                GroupBox {
                    title: "ToggleButton"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    GridLayout {
                        columns: Math.max(1, !!children[0] ? Math.floor(scrollView.width / children[0].implicitWidth - 0.5) : children.length)
                        ToggleButton {
                            text: "Pump 1"
                            checked: true
                        }
                        ToggleButton {
                            text: "Pump 2"
                            checked: false
                        }
                    }
                }

                GroupBox {
                    title: "StatusIndicator"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    GridLayout {
                        columns: Math.max(1, Math.floor(scrollView.width / recordButton.implicitWidth - 0.5))
                        columnSpacing: layoutSpacing * 4

                        Button {
                            id: recordButton
                            text: "Record"
                            Layout.alignment: Qt.AlignTop
                            onClicked: recordStatusIndicator.active = !recordStatusIndicator.active

                            StatusIndicator {
                                id: recordStatusIndicator
                                active: false
                                anchors.left: parent.left
                                anchors.leftMargin: Math.max(6, Math.round(text.implicitHeight * 0.4))
                                anchors.verticalCenter: parent.verticalCenter
                                rotation: 90
                            }
                        }
                        ColumnLayout {
                            Repeater {
                                model: 3
                                delegate: RowLayout {
                                    Layout.alignment: Qt.AlignCenter
                                    StatusIndicator {
                                        active: true
                                        color: "#f09116"
                                    }
                                    Label {
                                        text: "Camera " + (index + 1)
                                        font.family: Flat.FlatStyle.fontFamily
                                        renderType: Text.QtRendering
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    property Component progressComponent: ScrollView {
        id: scrollView
        horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff
        Flickable {
            anchors.fill: parent
            contentWidth: viewport.width
            contentHeight: progresscolumn.implicitHeight + textMargins * 1.5
            ColumnLayout {
                id: progresscolumn
                anchors.fill: parent
                anchors.leftMargin: textMargins
                anchors.rightMargin: textMargins
                anchors.bottomMargin: textMargins
                anchors.topMargin: textMargins / 2
                spacing: textMargins / 2
                enabled: !settingsData.allDisabled

                GroupBox {
                    title: "BusyIndicator"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    BusyIndicator {
                        id: busyindicator
                        anchors.centerIn: parent
                        running: scrollView.visible
                    }
                }

                GroupBox {
                    title: "ProgressBar"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        ProgressBar {
                            value: slider.value
                            maximumValue: slider.maximumValue
                            Layout.fillWidth: true
                        }
                        ProgressBar {
                            indeterminate: true
                            value: slider.value
                            maximumValue: slider.maximumValue
                            Layout.fillWidth: true
                        }
                    }
                }

                GroupBox {
                    title: "Slider"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        Slider {
                            id: slider
                            // TODO: can't use maximumValue / 2 here, otherwise the gauges
                            // initially show up as empty; find out why.
                            value: 50
                            // If we use the default value of 1 here, we run into QTBUG-42358,
                            // even though that report specifically uses 100 as an example...
                            maximumValue: 100
                            Layout.fillWidth: true
                        }
                    }
                }

                GroupBox {
                    title: "Gauge"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    Gauge {
                        id: gauge
                        value: slider.value * 1.4
                        orientation: window.width < window.height ? Qt.Vertical : Qt.Horizontal
                        minimumValue: slider.minimumValue * 1.4
                        maximumValue: slider.maximumValue * 1.4
                        tickmarkStepSize: 20

                        anchors.centerIn: parent
                    }
                }

                GroupBox {
                    title: "CircularGauge"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    CircularGauge {
                        id: circularGauge
                        value: slider.value * 3.2
                        minimumValue: slider.minimumValue * 3.2
                        maximumValue: slider.maximumValue * 3.2

                        anchors.centerIn: parent
                        width: Math.min(implicitWidth, parent.width)
                        height: Math.min(implicitHeight, parent.height)

                        style: Flat.CircularGaugeStyle {
                            tickmarkStepSize: 20
                            labelStepSize: 40
                            minorTickmarkCount: 2
                        }

                        Column {
                            anchors.centerIn: parent

                            Label {
                                text: Math.floor(circularGauge.value)
                                anchors.horizontalCenter: parent.horizontalCenter
                                renderType: Text.QtRendering
                                font.pixelSize: unitLabel.font.pixelSize * 2
                                font.family: Flat.FlatStyle.fontFamily
                                font.weight: Font.Light
                            }
                            Label {
                                id: unitLabel
                                text: "km/h"
                                renderType: Text.QtRendering
                                font.family: Flat.FlatStyle.fontFamily
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }
                }
            }
        }
    }

    property Component inputComponent: ScrollView {
        id: scrollView
        horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff
        Flickable {
            anchors.fill: parent
            contentWidth: viewport.width
            contentHeight: inputcolumn.implicitHeight + textMargins * 1.5
            ColumnLayout {
                id: inputcolumn
                anchors.fill: parent
                anchors.margins: textMargins
                spacing: textMargins / 2
                enabled: !settingsData.allDisabled

                GroupBox {
                    title: "TextField"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        TextField {
                            z: 1
                            placeholderText: "TextField"
                            Layout.fillWidth: true
                        }
                        TextField {
                            placeholderText: "Password"
                            echoMode: TextInput.Password // TODO: PasswordEchoOnEdit
                            Layout.fillWidth: true
                        }
                    }
                }

                GroupBox {
                    title: "ComboBox"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    visible: Qt.application.supportsMultipleWindows
                    ColumnLayout {
                        anchors.fill: parent
                        ComboBox {
                            model: ["Option 1", "Option 2", "Option 3"]
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            editable: true
                            model: ListModel {
                                id: combomodel
                                ListElement { text: "Option 1" }
                                ListElement { text: "Option 2" }
                                ListElement { text: "Option 3" }
                            }
                            onAccepted: {
                                if (find(currentText) === -1) {
                                    combomodel.append({text: editText})
                                    currentIndex = find(editText)
                                }
                            }
                            Layout.fillWidth: true
                        }
                    }
                }

                GroupBox {
                    title: "SpinBox"
                    checkable: settingsData.checks
                    flat: !settingsData.frames
                    Layout.fillWidth: true
                    GridLayout {
                        anchors.fill: parent
                        columns: Math.max(1, Math.floor(scrollView.width / spinbox.implicitWidth - 0.5))
                        SpinBox {
                            id: spinbox
                            Layout.fillWidth: true
                        }
                        SpinBox {
                            decimals: 1
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    Component {
        id: tableViewComponent
        TableView {
            id: view
            enabled: !settingsData.allDisabled
            TableViewColumn {
                role: "title"
                title: "Title"
                width: view.width / 2
                resizable: false
                movable: false
            }
            TableViewColumn {
                role: "author"
                title: "Author"
                width: view.width / 2
                resizable: false
                movable: false
            }

            frameVisible: false
            backgroundVisible: true
            alternatingRowColors: false
            model: ListModel {
                ListElement {
                    title: "Moby-Dick"
                    author: "Herman Melville"
                }
                ListElement {
                    title: "The Adventures of Tom Sawyer"
                    author: "Mark Twain"
                }
                ListElement {
                    title: "Cat’s Cradle"
                    author: "Kurt Vonnegut"
                }
                ListElement {
                    title: "Farenheit 451"
                    author: "Ray Bradbury"
                }
                ListElement {
                    title: "It"
                    author: "Stephen King"
                }
                ListElement {
                    title: "On the Road"
                    author: "Jack Kerouac"
                }
                ListElement {
                    title: "Of Mice and Men"
                    author: "John Steinbeck"
                }
                ListElement {
                    title: "Do Androids Dream of Electric Sheep?"
                    author: "Philip K. Dick"
                }
                ListElement {
                    title: "Uncle Tom’s Cabin"
                    author: "Harriet Beecher Stowe"
                }
                ListElement {
                    title: "The Call of the Wild"
                    author: "Jack London"
                }
                ListElement {
                    title: "The Old Man and the Sea"
                    author: "Ernest Hemingway"
                }
                ListElement {
                    title: "A Streetcar Named Desire"
                    author: "Tennessee Williams"
                }
                ListElement {
                    title: "Catch-22"
                    author: "Joseph Heller"
                }
                ListElement {
                    title: "One Flew Over the Cuckoo’s Nest"
                    author: "Ken Kesey"
                }
                ListElement {
                    title: "The Murders in the Rue Morgue"
                    author: "Edgar Allan Poe"
                }
                ListElement {
                    title: "Breakfast at Tiffany’s"
                    author: "Truman Capote"
                }
                ListElement {
                    title: "Death of a Salesman"
                    author: "Arthur Miller"
                }
                ListElement {
                    title: "Post Office"
                    author: "Charles Bukowski"
                }
                ListElement {
                    title: "Herbert West—Reanimator"
                    author: "H. P. Lovecraft"
                }
            }
        }
    }
    Component {
        id: calendarComponent
        Item {
            enabled: !settingsData.allDisabled
            Calendar {
                anchors.centerIn: parent
                weekNumbersVisible: true
                frameVisible: settingsData.frames
            }
        }
    }
    Component {
        id: textAreaComponent
        TextArea {
            enabled: !settingsData.allDisabled
            frameVisible: false
            flickableItem.flickableDirection: Flickable.VerticalFlick
            text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vestibulum quis justo a sem faucibus mattis nec vitae nisi. Fusce fringilla nulla a tellus vehicula sodales. Etiam volutpat suscipit erat vitae adipiscing. Sed vestibulum massa nisl, eget posuere urna porta ac. Morbi at nunc ligula. Cras et mauris aliquet ligula sodales suscipit eget imperdiet augue. Ut eget dui eu magna malesuada imperdiet. Donec imperdiet urna eu consequat ornare. Cras at metus tristique, ullamcorper nisl ut, faucibus mauris. Fusce in euismod arcu. Donec tristique rutrum porta. Praesent mattis ac tortor quis scelerisque. Integer luctus nulla ut lacinia tempus."
        }
    }
    Component {
        id: pieMenuComponent
        Item {
            enabled: !settingsData.allDisabled

            Column {
                anchors.fill: parent
                anchors.bottom: parent.bottom
                anchors.bottomMargin: controlData.textMargins
                spacing: Math.round(controlData.textMargins * 0.5)

                Image {
                    id: pieMenuImage
                    source: "qrc:/images/piemenu-image-rgb.jpg"
                    fillMode: Image.PreserveAspectFit
                    width: parent.width
                    height: Math.min((width / sourceSize.width) * sourceSize.height, (parent.height - parent.spacing) * 0.88)
                }
                Item {
                    width: parent.width
                    height: parent.height - pieMenuImage.height - parent.spacing

                    Text {
                        id: instructionText
                        anchors.fill: parent
                        anchors.leftMargin: controlData.textMargins
                        anchors.rightMargin: controlData.textMargins
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: "Tap and hold to open menu"
                        font.family: Flat.FlatStyle.fontFamily
                        font.pixelSize: Math.round(20 * Flat.FlatStyle.scaleFactor)
                        fontSizeMode: Text.Fit
                        color: Flat.FlatStyle.lightFrameColor
                    }
                }
            }
            MouseArea {
                id: mouseArea
                anchors.fill: parent
                onPressAndHold: pieMenu.popup(mouse.x, mouse.y)
            }
            PieMenu {
                id: pieMenu
                triggerMode: TriggerMode.TriggerOnClick

                MenuItem {
                    iconSource: "qrc:/images/piemenu-rgb-" + (pieMenu.currentIndex === 0 ? "pressed" : "normal") + ".png"
                    onTriggered: pieMenuImage.source = "qrc:/images/piemenu-image-rgb.jpg"
                }
                MenuItem {
                    iconSource: "qrc:/images/piemenu-bw-" + (pieMenu.currentIndex === 1 ? "pressed" : "normal") + ".png"
                    onTriggered: pieMenuImage.source = "qrc:/images/piemenu-image-bw.jpg"
                }
                MenuItem {
                    iconSource: "qrc:/images/piemenu-sepia-" + (pieMenu.currentIndex === 2 ? "pressed" : "normal") + ".png"
                    onTriggered: pieMenuImage.source = "qrc:/images/piemenu-image-sepia.jpg"
                }
            }
        }
    }
    Component {
        id: delayButtonComponent
        Item {
            enabled: !settingsData.allDisabled
            DelayButton {
                text: progress < 1 ? "START" : "STOP"
                anchors.centerIn: parent
            }
        }
    }
    Component {
        id: dialComponent
        Item {
            enabled: !settingsData.allDisabled
            Dial {
                anchors.centerIn: parent
            }
        }
    }
    Component {
        id: tumblerComponent
        Item {
            enabled: !settingsData.allDisabled
            Tumbler {
                anchors.centerIn: parent
                TumblerColumn {
                    model: {
                        var hours = [];
                        for (var i = 1; i <= 24; ++i)
                            hours.push(i < 10 ? "0" + i : i);
                        hours;
                    }
                }
                TumblerColumn {
                    model: {
                        var minutes = [];
                        for (var i = 0; i < 60; ++i)
                            minutes.push(i < 10 ? "0" + i : i);
                        minutes;
                    }
                }
            }
        }
    }
}
