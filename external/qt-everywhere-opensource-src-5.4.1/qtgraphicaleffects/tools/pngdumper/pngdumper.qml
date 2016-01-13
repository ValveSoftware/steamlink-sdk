/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module.
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
import org.qtproject.pngdumper 1.0

Rectangle {
    id: main
    width: 300
    height: width
    property real size: width
    property real timerInterval: 1
    property color background: "white"
    property bool checkerboard: false
    color: background

    Rectangle {
        id: blurMask
        anchors.fill: parent
        visible: false
        gradient: Gradient {
                 GradientStop { position: 0.3; color: "#ff000000" }
                 GradientStop { position: 0.7; color: "#00000000" }
        }
    }


    Gradient {
        id: firstGradient
        GradientStop { position: 0.000; color: Qt.rgba(1, 0, 0, 1) }
        GradientStop { position: 0.167; color: Qt.rgba(1, 1, 0, 1) }
        GradientStop { position: 0.333; color: Qt.rgba(0, 1, 0, 1) }
        GradientStop { position: 0.500; color: Qt.rgba(0, 1, 1, 1) }
        GradientStop { position: 0.667; color: Qt.rgba(0, 0, 1, 1) }
        GradientStop { position: 0.833; color: Qt.rgba(1, 0, 1, 1) }
        GradientStop { position: 1.000; color: Qt.rgba(1, 0, 0, 1) }
    }
    Gradient {
        id: secondGradient
        GradientStop { position: 0.0; color: "#F0F0F0" }
        GradientStop { position: 0.5; color: "#000000" }
        GradientStop { position: 1.0; color: "#F0F0F0" }
    }
    Gradient {
        id: thirdGradient
        GradientStop { position: 0.0; color: "#00000000" }
        GradientStop { position: 1.0; color: "#FF000000" }
    }

    ItemModel {
        id: itemModel
    }

    Item {
        id: container
        width: size
        height: size
        clip: true

        Image {
            source: '../../tests/manual/testbed/images/checker.png'
            smooth: true
            anchors.fill: parent
            fillMode: Image.Tile
            visible: main.checkerboard
        }

        ListView {
            id: list
            width: size
            height: size
            model: itemModel
            highlightMoveDuration: 1
            spacing: size
            anchors.centerIn: parent
            cacheBuffer: size
        }
    }

    // Sources
    Image {
        id: bug
        source: "../../tests/manual/testbed/images/bug.jpg"
        smooth: true
        width: size
        height: width
        fillMode: Image.PreserveAspectFit
        visible: false
    }
    ShaderEffectSource {
        id: butterfly
        smooth: true
        width: size
        height: width
        visible: false
        sourceItem:
            Image {
            //id: butterfly
            source: "../../tests/manual/testbed/images/butterfly.png"
            smooth: true
            width: size
            height: width
            fillMode: Image.PreserveAspectFit
            visible: false
        }
    }
    Image {
        id: fog
        source: "../../tests/manual/testbed/images/fog.png"
        smooth: true
        width: size
        height: width
        fillMode: Image.PreserveAspectFit
        visible: false
    }
    Rectangle {
        id: rect
        width: size / 2
        height: size / 2
        anchors.centerIn: container
        color: "#dde4ff"
        visible: false
    }
    Rectangle {
        id: displacementMapSource
        visible: false
        color: Qt.rgba(0.5, 0.5, 1.0, 1.0)
        smooth: true
        anchors.fill: bug
        Image {
            id: di
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            sourceSize: Qt.size(128, 128)
            source: "../../tests/manual/testbed/images/glass_normal.png"
            smooth: true
        }
    }

    ItemCapturer {
        id: capturer
    }

    Timer {
        interval: 1000
        running: true
        onTriggered: {
            // Ugly workaround for listview not updating itself
            list.contentX = 1
            eval("list.currentItem."+list.currentItem.__varyingProperty+"=list.currentItem.__values[0]");
            list.contentX = 0
            if (list.currentItem.init != undefined) list.currentItem.init()
            timer.running = true
        }
    }


    property int i: 0
    Connections {
        target: capturer
        onImageSaved: {
            if (i >= list.currentItem.__values.length - 1) {
                var filename = list.currentItem.__name + "_" + list.currentItem.__varyingProperty + (i + 1) + ".png"
                filename = filename.replace(/\"/g,"")
                capturer.document("\\table\n")
                capturer.document("\\header\n")
                capturer.document("\\o Output examples with different " + list.currentItem.__varyingProperty + " values\n")
                capturer.document("\\o\n\\o\n\\row\n")
                var ii = 0
                for(ii = 0; ii < list.currentItem.__values.length; ii++) {
                    filename = list.currentItem.__name + "_" + list.currentItem.__varyingProperty + (ii + 1) + ".png"
                    filename = filename.replace(/\"/g,"")
                    //filename = filename.toLowerCase();
                    capturer.document("    \\o \\image " + filename + "\n")
                }
                capturer.document("\\row\n")
                for(ii = 0; ii < list.currentItem.__values.length; ii++) {
                    capturer.document("    \\o \\bold { " + list.currentItem.__varyingProperty + ": " + list.currentItem.__values[ii] + " }\n")
                }
                for (var p = 0; p < list.currentItem.__properties.length; p++) {
                    if (list.currentItem.__properties[p] == list.currentItem.__varyingProperty) continue;

                    capturer.document("\\row\n")

                    for(ii = 0; ii < list.currentItem.__values.length; ii++) {
                        capturer.document("    \\o \\l " + list.currentItem.__properties[p] + ": " + eval("list.currentItem."+list.currentItem.__properties[p]) + "\n")
                    }
                }
                capturer.document("\\endtable\n")

                if (list.currentIndex < list.count-1) {
                    i = 0
                    if (list.currentItem.uninit != undefined) list.currentItem.uninit()
                    list.currentIndex++
                    list.positionViewAtIndex(list.currentIndex, ListView.Center)
                    if (list.currentItem.init != undefined) list.currentItem.init()
                } else Qt.quit()
            } else i++

            eval("list.currentItem."+list.currentItem.__varyingProperty+"=list.currentItem.__values[i]");
            timer.running = true
        }
    }

    Timer {
        id: timer
        interval: timerInterval
        running: false
        onTriggered: {
            var filename = list.currentItem.__name + "_" + list.currentItem.__varyingProperty + (i + 1) + ".png"
            filename = filename.replace(/\"/g,"")
            capturer.grabItem(container, filename)
        }
    }
}
