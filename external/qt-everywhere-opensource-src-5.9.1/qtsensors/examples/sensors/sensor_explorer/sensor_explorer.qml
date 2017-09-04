/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

import QtQuick 2.1
import QtQuick.Window 2.1
import QtQuick.Controls 1.0

//! [0]
import Explorer 1.0
//! [0]

Window {
    id: window
    width: 320
    height: 480
    minimumWidth: 320
    minimumHeight: 480

    //! [1]
    SensorExplorer {
        id: explorer
    }
    //! [1]

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        GroupBox {
            title: qsTr("Available Sensors")
            width: parent.width
            height: window.height * 0.4

            TableView {
                id: sensorList
                anchors.fill: parent
                //! [2]
                model: explorer.availableSensors
                //! [2]

                TableViewColumn { role: "id"; title: qsTr("ID"); width: sensorList.width * 0.7 }
                TableViewColumn { role: "start"; title: qsTr("Running"); width: sensorList.width * 0.3 - 5 }

                onClicked: {
                    explorer.selectedSensorItem = explorer.availableSensors[row]
                    //! [3]
                    propertyList.model = explorer.selectedSensorItem.properties
                    //! [3]
                    button.update()
                }
            }

            Button {
                id: button
                anchors.margins: 4
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: sensorList.bottom
                text: qsTr("Start")
                enabled: explorer.selectedSensorItem !== null

                function update() {
                    text = (explorer.selectedSensorItem !== null ?
                                (explorer.selectedSensorItem.start === true ?
                                     qsTr("Stop") : qsTr("Start")) : qsTr("Start"))
                }

                onClicked: {
                    if (explorer.selectedSensorItem !== null) {
                        //! [5]
                        if (text === "Start") {
                            explorer.selectedSensorItem.start = true;
                            text = "Stop";
                        }
                        else {
                            explorer.selectedSensorItem.start = false;
                            text = "Start";
                        }
                        //! [5]
                    }
                }
            }

        }

        GroupBox {
            title: qsTr("Properties")
            width: parent.width
            height: window.height * 0.55

            enabled: explorer.selectedSensorItem != null

            TableView {
                id: propertyList
                property PropertyInfo selectedItem: null

                anchors.fill: parent
                TableViewColumn { role: "name"; title: qsTr("Name"); width: propertyList.width * 0.5 }
                TableViewColumn { role: "value"; title: qsTr("Value"); width: propertyList.width * 0.5 - 5 }

                onClicked: {
                    selectedItem = model[row]
                }

                itemDelegate: {
                    if (selectedItem && selectedItem.isWriteable)
                        return editableDelegate;
                    return readOnlyDelegate;
                }

                Component {
                    id: readOnlyDelegate
                    Item {
                        Text {
                            width: parent.width
                            anchors.margins: 4
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            elide: styleData.elideMode
                            text: styleData.value
                            color: propertyList.model[styleData.row].isWriteable ?
                                       styleData.textColor : Qt.lighter(styleData.textColor)
                        }
                    }
                }

                Component {
                    id: editableDelegate
                    Item {
                        Text {
                            width: parent.width
                            anchors.margins: 4
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            elide: styleData.elideMode
                            text: styleData.value
                            color: styleData.textColor
                            visible: !styleData.selected || styleData.column === 0
                        }
                        Loader { // Initialize text editor lazily to improve performance
                            id: loaderEditor
                            anchors.margins: 4
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            Connections {
                                target: loaderEditor.item
                                onAccepted: {
                                    //! [4]
                                    explorer.selectedSensorItem.changePropertyValue(propertyList.selectedItem, loaderEditor.item.text);
                                    //! [4]
                                }
                            }

                            // Load the editor for selected 'Value' cell
                            sourceComponent: (styleData.selected && styleData.column === 1) ? editor : null

                            Component {
                                id: editor
                                TextInput {
                                    id: textinput
                                    color: styleData.textColor
                                    text: styleData.value
                                    MouseArea {
                                        id: mouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: textinput.forceActiveFocus()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
