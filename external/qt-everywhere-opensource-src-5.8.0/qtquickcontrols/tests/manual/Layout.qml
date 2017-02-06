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
import QtQuick.Layouts 1.0

Item {
    id: window
    width: 600
    height: 400

    property real defaultWidth: 30
    property real defaultHeight: 30

    Column {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        Row {
            width: parent.width
            height: 20
            CheckBox {
                id: ckHideGreen
                text: "Hide green rectangles"
                checked: false
                height: parent.height
            }
            Slider {
                id: spacingSlider
                value: 4
                minimumValue: 0
                maximumValue: 100
                width: 200
                height: parent.height
            }

        }
        TabView {
            id:frame
            width: parent.width
            height: window.height - ckHideGreen.height

            Tab {
                title: "Horizontal"

                Column {
                    spacing: 4

                    anchors {
                        top: parent.top
                        left: parent.left
                        right: parent.right
                        margins: 10
                    }

                    // [1]
                    RowLayout {
                        height: defaultHeight
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: spacingSlider.value

                        Rectangle {
                            color: "red"
                            width: 50
                            height: parent.height
                        }
                        Rectangle {
                            color: "green"
                            width: 100
                            height: parent.height
                            visible: !ckHideGreen.checked
                        }
                        Rectangle {
                            color: "blue"
                            width: 200
                            height: parent.height
                        }
                    }

                    // [2]
                    RowLayout {
                        height: defaultHeight
                        anchors.left: parent.left
                        anchors.right: parent.right

                        spacing: spacingSlider.value

                        Rectangle {
                            color: "red"
                            width: 40
                            height: parent.height
                        }
                        Rectangle {
                            color: "green"
                            width: 40
                            height: parent.height
                            visible: !ckHideGreen.checked
                        }
                        Item {
                            implicitWidth: 10
                        }
                        Rectangle {
                            color: "blue"
                            width: 40
                            height: parent.height
                        }
                    }

                    // [3]
                    RowLayout {
                        height: defaultHeight
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: spacingSlider.value

                        Rectangle {
                            color: "red"
                            height: parent.height
                            Layout.minimumWidth: 50
                            Layout.maximumWidth: 100
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            color: "green"
                            height: parent.height
                            visible: !ckHideGreen.checked
                            Layout.minimumWidth: 100
                            Layout.maximumWidth: 200
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            color: "blue"
                            height: parent.height
                            Layout.minimumWidth: 200
                            Layout.maximumWidth: 400
                            Layout.fillWidth: true
                        }
                    }

                    // [4]
                    RowLayout {
                        spacing: 100 + spacingSlider.value

                        height: defaultHeight
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Rectangle {
                            color: "red"
                            height: parent.height
                            Layout.minimumWidth: 100
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            color: "green"
                            height: parent.height
                            visible: !ckHideGreen.checked
                            Layout.minimumWidth: 200
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            color: "blue"
                            height: parent.height
                            Layout.minimumWidth: 300
                            Layout.fillWidth: true
                        }
                    }

                    // [5]
                    RowLayout {
                        spacing: spacingSlider.value
                        height: defaultHeight
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Rectangle {
                            color: "red"
                            height: parent.height
                            Layout.minimumWidth: 200
                            Layout.maximumWidth: 500
                            Layout.fillWidth: true
                        }
                    }

                    // [6]
                    RowLayout {
                        spacing: 40 + spacingSlider.value
                        height: defaultHeight
                        anchors.left: parent.left
                        anchors.right: parent.right

                        RowLayout {
                            spacing: 10
                            height: parent.height

                            Rectangle {
                                color: "red"
                                height: parent.height
                                Layout.minimumWidth: 100
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                color: "blue"
                                height: parent.height
                                Layout.minimumWidth: 200
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            spacing: 10
                            height: parent.height

                            Rectangle {
                                color: "green"
                                height: parent.height
                                width: 40
                                visible: !ckHideGreen.checked
                                Layout.maximumWidth: 300
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                color: "red"
                                height: parent.height
                                Layout.minimumWidth: 40
                                Layout.maximumWidth: 100
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }


            Tab {
                title: "Vertical"

                Row {
                    spacing: 4

                    anchors {
                        top: parent.top

                        left: parent.left
                        bottom: parent.bottom
                        margins: 10
                    }

                    // [1]
                    ColumnLayout {
                        width: defaultWidth
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        spacing: spacingSlider.value

                        Rectangle {
                            color: "red"
                            width: parent.width
                            height: 40
                        }
                        Rectangle {
                            color: "green"
                            width: parent.width
                            height: 80
                            visible: !ckHideGreen.checked
                        }
                        Rectangle {
                            color: "blue"
                            width: parent.width
                            height: 120
                        }
                    }

                    // [2]
                    ColumnLayout {
                        width: defaultWidth
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        spacing: spacingSlider.value

                        Rectangle {
                            color: "red"
                            width: parent.width
                            height: 40
                        }
                        Rectangle {
                            color: "green"
                            width: parent.width
                            visible: !ckHideGreen.checked
                            height: 80
                        }
                        Item {
                            implicitHeight: 10
                        }
                        Rectangle {
                            color: "blue"
                            width: parent.width
                            height: 40
                        }
                    }

                    // [3]
                    ColumnLayout {
                        width: defaultWidth
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        spacing: spacingSlider.value

                        Rectangle {
                            color: "red"
                            width: parent.width
                            height: 60
                            Layout.minimumHeight: 50
                            Layout.maximumHeight: 100
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "green"
                            width: parent.width
                            height: 90
                            visible: !ckHideGreen.checked
                            Layout.minimumHeight: 75
                            Layout.maximumHeight: 125
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "blue"
                            width: parent.width
                            height: 120
                            Layout.minimumHeight: 100
                            Layout.fillHeight: true
                        }
                    }

                    // [4]
                    ColumnLayout {
                        spacing: 100 + spacingSlider.value
                        width: defaultWidth
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        Rectangle {
                            color: "red"
                            width: parent.width
                            Layout.minimumHeight: 100
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "green"
                            width: parent.width
                            visible: !ckHideGreen.checked
                            Layout.minimumHeight: 200
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "blue"
                            width: parent.width
                            Layout.minimumHeight: 300
                            Layout.fillHeight: true
                        }
                    }

                    // [5]
                    ColumnLayout {
                        spacing: 40 + spacingSlider.value
                        width: defaultWidth
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        ColumnLayout {
                            spacing: 10
                            width: parent.width
                            Layout.fillHeight: true

                            Rectangle {
                                color: "red"
                                width: parent.width
                                Layout.minimumHeight: 100
                                Layout.maximumHeight: 300
                                Layout.fillHeight: true
                            }
                            Rectangle {
                                color: "blue"
                                width: parent.width
                                Layout.minimumHeight: 100
                                Layout.fillHeight: true
                                Layout.maximumHeight: 200
                            }
                        }

                        ColumnLayout {
                            spacing: 10
                            width: parent.width
                            Layout.maximumHeight: 200
                            Layout.fillHeight: true
                            Rectangle {
                                color: "green"
                                width: parent.width
                                height: 50
                                visible: !ckHideGreen.checked
                                Layout.maximumHeight: 300
                                Layout.fillHeight: true
                            }
                            Rectangle {
                                color: "red"
                                width: parent.width
                                height: 50
                                Layout.minimumHeight: 40
                                Layout.fillHeight: true
                            }
                        }
                    }
                }
            }

            Tab {
                title: "Horizontal and Vertical"

                ColumnLayout {
                    anchors.fill: parent

                    // [1]
                    RowLayout {
                        height: defaultHeight
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Layout.minimumHeight: 100

                        Rectangle {
                            color: "red"
                            height: parent.height
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            color: "green"
                            height: parent.height
                            visible: !ckHideGreen.checked
                            implicitWidth: 100
                        }
                        Rectangle {
                            color: "blue"
                            height: parent.height
                            Layout.fillWidth: true
                        }
                    }

                    // [2]
                    Rectangle {
                        color: "yellow"
                        height: parent.height
                        anchors.left: parent.left
                        anchors.right: parent.right
                        Layout.minimumHeight: 10
                        Layout.maximumHeight: 30
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    // [3]
                    RowLayout {
                        height: defaultHeight
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Rectangle {
                            color: "red"
                            height: parent.height
                            Layout.maximumHeight: 200
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            color: "blue"
                            height: parent.height

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10
                                opacity: 0.6

                                Rectangle {
                                    color: "darkRed"
                                    height: parent.height
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    Layout.minimumHeight: 100
                                    Layout.maximumHeight: 200
                                    Layout.fillHeight: true
                                }

                                Rectangle {
                                    color: "darkGreen"
                                    height: parent.height
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    Layout.fillHeight: true
                                }
                            }
                        }
                    }
                }
            }

            Tab {
                title: "Grid"
                Column {
                    spacing: 4

                    anchors {
                        top: parent.top
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        margins: 10
                    }

                    // [1]
                    GridLayout {
                        id: flag
                        rowSpacing: spacingSlider.value
                        columnSpacing: spacingSlider.value
                        // row 0
                        anchors.left: parent.left
                        anchors.right: parent.right
                        Rectangle {
                            color: "red"
                            width: 52
                            height: 52
                            Layout.row: 0
                            Layout.column: 0
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "white"
                            width: 8
                            height: 52
                            Layout.row: 0
                            Layout.column: 1
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "blue"
                            width: 18
                            height: 60
                            Layout.row: 0
                            Layout.column: 2
                            Layout.rowSpan: 2
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "white"
                            width: 8
                            height: 52
                            Layout.row: 0
                            Layout.column: 3
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "red"
                            width: 140
                            height: 52
                            Layout.row: 0
                            Layout.column: 4
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }

                        // row 1
                        Rectangle {
                            color: "white"
                            width: 60
                            height: 8
                            Layout.row: 1
                            Layout.column: 0
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "white"
                            width: 60
                            height: 8
                            Layout.row: 1
                            Layout.column: 3
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }

                        // row 2
                        Rectangle {
                            color: "blue"
                            width: 226
                            height: 18
                            Layout.row: 2
                            Layout.column: 0
                            Layout.columnSpan: 5
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }

                        // row 3
                        Rectangle {
                            color: "white"
                            width: 60
                            height: 8
                            Layout.row: 3
                            Layout.column: 0
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "blue"
                            width: 18
                            height: 60
                            Layout.row: 3
                            Layout.column: 2
                            Layout.rowSpan: 2
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "white"
                            width: 140
                            height: 8
                            Layout.row: 3
                            Layout.column: 3
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }

                        // row 4
                        Rectangle {
                            color: "red"
                            width: 52
                            height: 52
                            Layout.row: 4
                            Layout.column: 0
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "white"
                            width: 8
                            height: 52
                            Layout.row: 4
                            Layout.column: 1
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "white"
                            width: 8
                            height: 52
                            Layout.row: 4
                            Layout.column: 3
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "red"
                            width: 140
                            height: 52
                            Layout.row: 4
                            Layout.column: 4
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                    }

                    Text {
                        text: "Norwegian flag"
                    }
                    // [1]
                    GridLayout {
                        columns: 4
                        flow: GridLayout.LeftToRight
                        anchors.left: parent.left
                        anchors.right: parent.right
                        Rectangle {
                            color: "green"
                            width: 20
                            height: 20
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "green"
                            width: 20
                            height: 20
                            Layout.row: 1
                            Layout.column: 1
                            Layout.rowSpan: 2
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "green"
                            width: 20
                            height: 20
                            Layout.row: 0
                            Layout.column: 1
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Rectangle {
                            color: "green"
                            width: 20
                            height: 20
                            Layout.rowSpan: 2
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Repeater {
                            model: 10
                            Rectangle {
                                color: Qt.rgba(1, 0, 0, 1 - (index/10.0))
                                width: 20
                                height: 20
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Text { text: index }
                            }
                        }
                        Rectangle {
                            color: "green"
                            width: 20
                            Layout.columnSpan:2
                            height: 20
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                    }
                }
            }
        }
    }
}
