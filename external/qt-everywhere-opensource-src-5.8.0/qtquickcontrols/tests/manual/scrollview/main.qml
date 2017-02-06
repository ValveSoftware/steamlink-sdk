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

ApplicationWindow {
    title: "Component Gallery"

    width: 580
    height: 400
    property string loremIpsum:
        "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor "+
        "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor "+
        "incididunt ut labore et dolore magna aliqua.\n Ut enim ad minim veniam, quis nostrud "+
        "exercitation ullamco laboris nisi ut aliquip ex ea commodo cosnsequat. ";

    SystemPalette {id: syspal}
    color: syspal.window

    toolBar: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            CheckBox {
                id: frameCheck
                text: "Frame"
                checked: true
                implicitWidth: 80
            }
            SpinBox {
                id : widthSpinBox
                maximumValue: 2000
                value: 1000
                implicitWidth: 80
            }
            SpinBox {
                id : heightSpinBox
                maximumValue: 2000
                value: 1000
                implicitWidth: 80
            }
            CheckBox {
                id: largeCheck
                text: "Large"
                checked: false
                implicitWidth: 80
            }
            Item { Layout.fillWidth: true }
        }
    }

    TabView {
        id:frame
        anchors.fill: parent
        anchors.margins: 4
        frameVisible: frameCheck.checked
        Tab {
            title: "Rectangle"
            ScrollView {
                anchors.fill: parent
                anchors.margins:4
                frameVisible: frameCheck.checked
                Rectangle {
                    width: widthSpinBox.value
                    height: heightSpinBox.value
                }
            }
        }
        Tab {
            title: "Image"
            ScrollView {
                anchors.fill: parent
                anchors.margins:4
                frameVisible: frameCheck.checked
                Image {
                    width: widthSpinBox.value
                    height: heightSpinBox.value
                    fillMode: Image.Tile
                    source: "../../../examples/quick/controls/touch/images/button_pressed.png"
                }
            }
        }
        Tab {
            title: "Flickable"
            ScrollView{
                anchors.fill: parent
                anchors.margins:4
                frameVisible: frameCheck.checked
                Flickable {
                    contentWidth: widthSpinBox.value
                    contentHeight: heightSpinBox.value
                    Image {
                        width: widthSpinBox.value
                        height: heightSpinBox.value
                        fillMode: Image.Tile
                        source: "../../../examples/quick/controls/touch/images/button_pressed.png"
                    }
                }
            }
        }
        Tab {
            title: "TextArea"
            TextArea {
                id: area
                anchors.margins:4
                frameVisible: frameCheck.checked
                text: loremIpsum + loremIpsum + loremIpsum + loremIpsum
                anchors.fill: parent
                font.pixelSize: largeCheck.checked ? 26 : 13
            }
        }
        Tab {
            title: "ListView"
            ScrollView{
                anchors.fill: parent
                anchors.margins:4
                frameVisible: frameCheck.checked
                ListView {
                    width: 400
                    model: 30
                    delegate: Rectangle {
                        width: parent.width
                        height: largeCheck.checked ? 60 : 30
                        Text {
                            anchors.fill: parent
                            anchors.margins: 4
                            text: modelData
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: "darkgray"
                        }
                    }
                }
            }
        }
        Tab {
            title: "TableView"
            TableView {
                id: view
                anchors.margins:4
                anchors.fill: parent
                model: 10
                frameVisible: frameCheck.checked

                rowDelegate: Rectangle {
                     width: parent.width
                     height: largeCheck.checked ? 60 : 30
                     Rectangle {
                         anchors.bottom: parent.bottom
                         width: parent.width
                         height: 1
                         color: "darkgray"
                     }
                 }

                TableViewColumn {title: "first"
                    width: view.viewport.width
                }
            }
        }
    }
}

