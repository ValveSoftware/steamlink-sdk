/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

ApplicationWindow {
    id: window
    property variant defaultAlignment: Qt.AlignBaseline
    title: "Layouts with baselines"
    property int margin: 11
    width: mainLayout.implicitWidth + 2 * margin
    height: mainLayout.implicitHeight + 2 * margin
    minimumWidth: mainLayout.Layout.minimumWidth + 2 * margin
    minimumHeight: mainLayout.Layout.minimumHeight + 2 * margin

    Component {
        id: visualBaselineComponent
        Rectangle {
            height: 1
            width: 1
            color: 'red'
        }
    }

    Item {
        id: visualBaselinesContainer
        width: parent.width
        z: 1
        opacity: 0.5

        function setBaselinesVisible(showBaselines) {
            for (var i = 0; i < visualBaselinesContainer.children.length; ++i) {
                visualBaselinesContainer.children[i].destroy();
            }
            if (showBaselines) {
                // assumes mainLayout/GroupBox/Layout/<child_items> hierarchy
                // Iterates over all <child_items> and gathers their baseline positions,
                var map_baselines = {}
                for (var i = 0; i < mainLayout.children.length; ++i) {
                    var grp = mainLayout.children[i]
                    var lay = grp.contentItem.children[0]
                    var y = mainLayout.y + grp.y + grp.contentItem.y + lay.y
                    var x = mainLayout.x + grp.x + grp.contentItem.x + lay.x
                    var w = lay.width
                    for (var j = 0; j < lay.children.length; ++j) {
                        var child = lay.children[j];
                        if (child.visible && child.baselineOffset > 0) {
                            var baseline = y + child.y + child.baselineOffset
                            map_baselines[baseline] = {x: x, width: w};
                        }
                    }
                }

                for (var key in map_baselines) {
                    var o = map_baselines[key];
                    var visualBaseline = visualBaselineComponent.createObject(visualBaselinesContainer, o);
                    visualBaseline.y = key;
                }
            }
        }
        Timer {
            // This is a kludge to wait until the layout has been rearranged, since that won't
            // happen until we get a polish event.
            // This will wait minimum of one full vertical scan (17 milliseconds), which
            // should usually be enough
            id: refreshBaselinesTimer
            running: false
            interval: 17
            onTriggered: {
                visualBaselinesContainer.setBaselinesVisible(ckShowBaselines.checked);
            }
        }
    }

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        anchors.margins: margin

        GroupBox {
            id: rowBoxTools
            title: "Developer tools"
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                CheckBox {
                    id: ckShowBaselines
                    text: "Show baselines"
                    checked: false
                    onCheckedChanged: {
                        visualBaselinesContainer.setBaselinesVisible(checked);
                    }
                }
                Label {
                    text: "Alignment:"
                }
                ComboBox {
                    model: ListModel {
                        id: cbItems
                        ListElement { text: "Default"; value: 0 }
                        ListElement { text: "Align Left"; value: Qt.AlignLeft }
                        ListElement { text: "Align Right"; value: Qt.AlignRight }
                        ListElement { text: "Align HCenter"; value: Qt.AlignHCenter }
                        ListElement { text: "Align Baseline"; value: Qt.AlignBaseline }
                        ListElement { text: "Align Top"; value: Qt.AlignTop }
                        ListElement { text: "Align Bottom"; value: Qt.AlignBottom }
                        ListElement { text: "Align VCenter"; value: Qt.AlignVCenter }
                    }
                    Component.onCompleted: {
                        for (var i = 0; i < cbItems.count; ++i) {
                            var v = cbItems.get(i).value;
                            if (v == defaultAlignment) {
                                currentIndex = i;
                                break;
                            }
                        }
                    }
                    onCurrentIndexChanged: {
                        // assumes mainLayout/GroupBox/Layout/<child_items> hierarchy
                        // Iterates over all <child_items> and modifies their baseline alignment
                        for (var i = 0; i < mainLayout.children.length; ++i) {
                            var grp = mainLayout.children[i]
                            var lay = grp.contentItem.children[0]
                            for (var j = 0; j < lay.children.length; ++j) {
                                var child = lay.children[j];
                                child.Layout.alignment = cbItems.get(currentIndex).value
                            }
                        }
                        refreshBaselinesTimer.start();
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
        GroupBox {
            id: rowBox
            title: "Row layout"
            Layout.fillWidth: true

            RowLayout {
                id: rowLayout
                anchors.fill: parent
                Repeater {
                    model: 3
                    Text {
                        text: "Typography"
                        font.pixelSize: 10 + 4*index
                        Layout.alignment: defaultAlignment
                        Layout.fillWidth: index == 2
                    }
                }
            }
        }

        GroupBox {
            id: gridBox
            title: "Grid layout"
            Layout.fillWidth: true

            GridLayout {
                id: gridLayout
                anchors.fill: parent
                rows: 2
                flow: GridLayout.TopToBottom
                Repeater {
                    model: 6
                    Text {
                        text: "Typography"
                        font.pixelSize: 8 + 4*index
                        Layout.alignment: defaultAlignment
                    }
                }
            }
        }


        GroupBox {
            id: rowBoxWithControls
            title: "Row layout with Controls"
            Layout.fillWidth: true

            RowLayout {
                id: rowLayoutWithControls
                anchors.fill: parent
                Label {
                    id: rowlabel
                    text: "Typo"
                    Layout.alignment: defaultAlignment
                }
                Button {
                    text: "Typo"
                    Layout.alignment: defaultAlignment
                }
                CheckBox {
                    text: "Typo"
                    Layout.alignment: defaultAlignment
                }
                ComboBox {
                    model: ["Typo"]
                    currentIndex: 0
                    Layout.alignment: defaultAlignment
                }
                RadioButton {
                    text: "Typo"
                    Layout.alignment: defaultAlignment
                }
                SpinBox {
                    value: 42
                    Layout.alignment: defaultAlignment
                }
                TextField {
                    text: "Typo"
                    Layout.alignment: defaultAlignment
                    Layout.maximumWidth: 40
                }
            }

        }

        GroupBox {
            id: gridBoxWithControls
            title: "Grid layout with Controls"
            Layout.fillWidth: true

            GridLayout {
                id: gridLayoutWithControls
                columns: 3
                flow: GridLayout.LeftToRight
                anchors.fill: parent
                Label {
                    text: "Typography"
                    Layout.alignment: defaultAlignment
                }
                Button {
                    text: "Typography"
                    Layout.alignment: defaultAlignment
                }
                CheckBox {
                    text: "Typography"
                    Layout.alignment: defaultAlignment
                }
                ComboBox {
                    model: ["Typography"]
                    currentIndex: 0
                    Layout.alignment: defaultAlignment
                }
                RadioButton {
                    text: "Typography"
                    Layout.alignment: defaultAlignment
                }
                SpinBox {
                    value: 42
                    Layout.alignment: defaultAlignment
                }
                TextField {
                    id: gridTextField
                    text: "Typography"
                    Layout.alignment: defaultAlignment
                }
            }
        }
    }
}
