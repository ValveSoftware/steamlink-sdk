/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.1
import QtDataVisualization 1.0
import "."

Rectangle {
    id: mainView
    width: 1280
    height: 720
    visible: true

    ListModel {
        id: graphModel
        ListElement{ xPos: 0.0; yPos: 0.0; zPos: 0.0; rotation: "0.92388, 0.220942, 0.220942, 0.220942"}
        ListElement{ xPos: 1.0; yPos: 1.0; zPos: 1.0; rotation: "@45,1.0,1.0,1.0" }
    }

    Timer {
        id: dataTimer
        interval: 1
        running: true
        repeat: true
        property bool isIncreasing: true
        property real rotationAngle: 0

        function generateQuaternion() {
            return "@" + Math.random() * 360 + "," + Math.random() + "," + Math.random() + "," + Math.random()
        }

        function appendRow() {
            graphModel.append({"xPos": Math.random(),
                                  "yPos": Math.random(),
                                  "zPos": Math.random(),
                                  "rotation": generateQuaternion()
                              });
        }

        onTriggered: {
            rotationAngle = rotationAngle + 1
            scatterSeries.setMeshAxisAndAngle(Qt.vector3d(1,1,1), rotationAngle)
            if (isIncreasing) {
                appendRow()
                appendRow()
                appendRow()
                appendRow()
                appendRow()
                appendRow()
                appendRow()
                appendRow()
                appendRow()
                appendRow()
                if (graphModel.count > 5000) {
                    scatterGraph.theme.type = Theme3D.ThemeIsabelle;
                    isIncreasing = false;
                }
            } else {
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                graphModel.remove(Math.random() * (graphModel.count - 1));
                if (graphModel.count == 2) {
                    scatterGraph.theme.type = Theme3D.ThemeDigia;
                    isIncreasing = true;
                }
            }
        }
    }

    ThemeColor {
        id: dynamicColor
        ColorAnimation on color {
            from: "red"
            to: "yellow"
            duration: 5000
            loops: Animation.Infinite
        }
    }

    Item {
        id: dataView
        anchors.bottom: parent.bottom
        width: parent.width
        height: parent.height - shadowToggle.height

        Scatter3D {
            id: scatterGraph
            width: dataView.width
            height: dataView.height
            theme: Theme3D {
                type: Theme3D.ThemeQt
                baseColors: [dynamicColor]
            }
            shadowQuality: AbstractGraph3D.ShadowQualitySoftMedium
            scene.activeCamera.yRotation: 30.0
            inputHandler: null
            axisX.min: 0
            axisY.min: 0
            axisZ.min: 0
            axisX.max: 1
            axisY.max: 1
            axisZ.max: 1

            Scatter3DSeries {
                id: scatterSeries
                itemLabelFormat: "X:@xLabel Y:@yLabel Z:@zLabel"
                mesh: Abstract3DSeries.MeshCube

                ItemModelScatterDataProxy {
                    itemModel: graphModel
                    xPosRole: "xPos"
                    yPosRole: "yPos"
                    zPosRole: "zPos"
                    rotationRole: "rotation"
                }
            }
        }

        MouseArea {
            id: inputArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            property int mouseX: -1
            property int mouseY: -1

            onPositionChanged: {
                mouseX = mouse.x;
                mouseY = mouse.y;
            }

            onWheel: {
                // Adjust zoom level based on what zoom range we're in.
                var zoomLevel = scatterGraph.scene.activeCamera.zoomLevel;
                if (zoomLevel > 100)
                    zoomLevel += wheel.angleDelta.y / 12.0;
                else if (zoomLevel > 50)
                    zoomLevel += wheel.angleDelta.y / 60.0;
                else
                    zoomLevel += wheel.angleDelta.y / 120.0;
                if (zoomLevel > 500)
                    zoomLevel = 500;
                else if (zoomLevel < 10)
                    zoomLevel = 10;

                scatterGraph.scene.activeCamera.zoomLevel = zoomLevel;
            }
        }

        Timer {
            id: reselectTimer
            interval: 10
            running: true
            repeat: true
            onTriggered: {
                scatterGraph.scene.selectionQueryPosition = Qt.point(inputArea.mouseX, inputArea.mouseY);
            }
        }
    }

    NumberAnimation {
        id: cameraAnimationX
        loops: Animation.Infinite
        running: true
        target: scatterGraph.scene.activeCamera
        property:"xRotation"
        from: 0.0
        to: 360.0
        duration: 20000
    }


    SequentialAnimation {
        id: cameraAnimationY
        loops: Animation.Infinite
        running: true

        NumberAnimation {
            target: scatterGraph.scene.activeCamera
            property:"yRotation"
            from: 5.0
            to: 45.0
            duration: 9000
            easing.type: Easing.InOutSine
        }

        NumberAnimation {
            target: scatterGraph.scene.activeCamera
            property:"yRotation"
            from: 45.0
            to: 5.0
            duration: 9000
            easing.type: Easing.InOutSine
        }
    }

    NewButton {
        id: shadowToggle
        width: parent.width / 3 // We're adding 3 buttons and want to divide them equally
        text: "Hide Shadows"
        anchors.left: parent.left

        onClicked: {
            if (scatterGraph.shadowQuality === AbstractGraph3D.ShadowQualityNone) {
                scatterGraph.shadowQuality = AbstractGraph3D.ShadowQualitySoftMedium;
                text = "Hide Shadows";
            } else {
                scatterGraph.shadowQuality = AbstractGraph3D.ShadowQualityNone;
                text = "Show Shadows";
            }
        }
    }

    NewButton {
        id: cameraToggle
        width: parent.width / 3
        text: "Pause Camera"
        anchors.left: shadowToggle.right

        onClicked: {
            cameraAnimationX.paused = !cameraAnimationX.paused;
            cameraAnimationY.paused = cameraAnimationX.paused;
            if (cameraAnimationX.paused) {
                text = "Animate Camera";
            } else {
                text = "Pause Camera";
            }
        }
    }

    NewButton {
        id: exitButton
        width: parent.width / 3
        text: "Quit"
        anchors.left: cameraToggle.right
        onClicked: Qt.quit(0);
    }
}
