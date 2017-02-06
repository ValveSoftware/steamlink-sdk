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
import QtDataVisualization 1.1
import "."

Item {
    id: mainView
    width: 800
    height: 600
    visible: true

    property int selectedAxisLabel: -1
    property real dragSpeedModifier: 100.0
    property int currentMouseX: -1
    property int currentMouseY: -1
    property int previousMouseX: -1
    property int previousMouseY: -1

    ListModel {
        id: graphModel
        ListElement{ xPos: 0.0; yPos: 0.0; zPos: 0.0; rotation: "@0,0,0,0" }
        ListElement{ xPos: 1.0; yPos: 1.0; zPos: 1.0; rotation: "@45,1,1,1" }
    }

    Timer {
        id: dataTimer
        interval: 1
        running: true
        repeat: true
        property bool isIncreasing: true
        property real rotationAngle: 0

        function generateQuaternion() {
            return "@" + Math.random() * 360 + "," + Math.random() + ","
                    + Math.random() + "," + Math.random()
        }

        function appendRow() {
            graphModel.append({"xPos": Math.random(),
                                  "yPos": Math.random(),
                                  "zPos": Math.random(),
                                  "rotation": generateQuaternion()
                              });
        }

        //! [10]
        onTriggered: {
            rotationAngle = rotationAngle + 1
            qtCube.setRotationAxisAndAngle(Qt.vector3d(1,0,1), rotationAngle)
            //! [10]
            scatterSeries.setMeshAxisAndAngle(Qt.vector3d(1,1,1), rotationAngle)
            if (isIncreasing) {
                for (var i = 0; i < 10; i++)
                    appendRow()
                if (graphModel.count > 2002) {
                    scatterGraph.theme = isabelleTheme
                    isIncreasing = false
                }
            } else {
                graphModel.remove(2, 10);
                if (graphModel.count == 2) {
                    scatterGraph.theme = dynamicColorTheme
                    isIncreasing = true
                }
            }
        }
    }

    ThemeColor {
        id: dynamicColor
        ColorAnimation on color {
            from: "red"
            to: "yellow"
            duration: 2000
            loops: Animation.Infinite
        }
    }

    Theme3D {
        id: dynamicColorTheme
        type: Theme3D.ThemeEbony
        baseColors: [dynamicColor]
        font.pointSize: 50
        labelBorderEnabled: true
        labelBackgroundColor: "gold"
        labelTextColor: "black"
    }

    Theme3D {
        id: isabelleTheme
        type: Theme3D.ThemeIsabelle
        font.pointSize: 50
        labelBorderEnabled: true
        labelBackgroundColor: "gold"
        labelTextColor: "black"
    }

    Item {
        id: dataView
        anchors.bottom: parent.bottom
        width: parent.width
        height: parent.height

        //! [0]
        Scatter3D {
            id: scatterGraph
            inputHandler: null
            //! [0]
            width: dataView.width
            height: dataView.height
            theme: dynamicColorTheme
            shadowQuality: AbstractGraph3D.ShadowQualityLow
            scene.activeCamera.yRotation: 45.0
            scene.activeCamera.xRotation: 45.0
            scene.activeCamera.zoomLevel: 75.0

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
            //! [9]
            customItemList: [
                Custom3DItem {
                    id: qtCube
                    meshFile: ":/mesh/cube"
                    textureFile: ":/texture/texture"
                    position: Qt.vector3d(0.65,0.35,0.65)
                    scaling: Qt.vector3d(0.3,0.3,0.3)
                }
            ]
            //! [9]
            //! [5]
            onSelectedElementChanged: {
                if (selectedElement >= AbstractGraph3D.ElementAxisXLabel
                        && selectedElement <= AbstractGraph3D.ElementAxisZLabel)
                    selectedAxisLabel = selectedElement
                else
                    selectedAxisLabel = -1
            }
            //! [5]
        }

        //! [1]
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            //! [1]

            //! [3]
            onPositionChanged: {
                currentMouseX = mouse.x;
                currentMouseY = mouse.y;
                //! [3]
                //! [6]
                if (pressed && selectedAxisLabel != -1)
                    dragAxis();
                //! [6]
                //! [4]
                previousMouseX = currentMouseX;
                previousMouseY = currentMouseY;
            }
            //! [4]

            //! [2]
            onPressed: {
                scatterGraph.scene.selectionQueryPosition = Qt.point(mouse.x, mouse.y);
            }
            //! [2]

            onReleased: {
                // We need to clear mouse positions and selected axis, because touch devices cannot
                // track position all the time
                selectedAxisLabel = -1
                currentMouseX = -1
                currentMouseY = -1
                previousMouseX = -1
                previousMouseY = -1
            }
        }
    }

    //! [7]
    function dragAxis() {
        // Do nothing if previous mouse position is uninitialized
        if (previousMouseX === -1)
            return

        // Directional drag multipliers based on rotation. Camera is locked to 45 degrees, so we
        // can use one precalculated value instead of calculating xx, xy, zx and zy individually
        var cameraMultiplier = 0.70710678

        // Calculate the mouse move amount
        var moveX = currentMouseX - previousMouseX
        var moveY = currentMouseY - previousMouseY

        // Adjust axes
        switch (selectedAxisLabel) {
        case AbstractGraph3D.ElementAxisXLabel:
            var distance = ((moveX - moveY) * cameraMultiplier) / dragSpeedModifier
            // Check if we need to change min or max first to avoid invalid ranges
            if (distance > 0) {
                scatterGraph.axisX.min -= distance
                scatterGraph.axisX.max -= distance
            } else {
                scatterGraph.axisX.max -= distance
                scatterGraph.axisX.min -= distance
            }
            break
        case AbstractGraph3D.ElementAxisYLabel:
            distance = moveY / dragSpeedModifier
            // Check if we need to change min or max first to avoid invalid ranges
            if (distance > 0) {
                scatterGraph.axisY.max += distance
                scatterGraph.axisY.min += distance
            } else {
                scatterGraph.axisY.min += distance
                scatterGraph.axisY.max += distance
            }
            break
        case AbstractGraph3D.ElementAxisZLabel:
            distance = ((moveX + moveY) * cameraMultiplier) / dragSpeedModifier
            // Check if we need to change min or max first to avoid invalid ranges
            if (distance > 0) {
                scatterGraph.axisZ.max += distance
                scatterGraph.axisZ.min += distance
            } else {
                scatterGraph.axisZ.min += distance
                scatterGraph.axisZ.max += distance
            }
            break
        }
    }
    //! [7]

    NewButton {
        id: rangeToggle
        width: parent.width / 3 // We're adding 3 buttons and want to divide them equally
        text: "Use Preset Range"
        anchors.left: parent.left
        property bool autoRange: true
        onClicked: {
            if (autoRange) {
                text = "Use Automatic Range"
                scatterGraph.axisX.min = 0.3
                scatterGraph.axisX.max = 0.7
                scatterGraph.axisY.min = 0.3
                scatterGraph.axisY.max = 0.7
                scatterGraph.axisZ.min = 0.3
                scatterGraph.axisZ.max = 0.7
                autoRange = false
                dragSpeedModifier = 200.0
            } else {
                text = "Use Preset Range"
                autoRange = true
                dragSpeedModifier = 100.0
            }
            scatterGraph.axisX.autoAdjustRange = autoRange
            scatterGraph.axisY.autoAdjustRange = autoRange
            scatterGraph.axisZ.autoAdjustRange = autoRange
        }
    }

    //! [8]
    NewButton {
        id: orthoToggle
        width: parent.width / 3
        text: "Display Orthographic"
        anchors.left: rangeToggle.right
        onClicked: {
            if (scatterGraph.orthoProjection) {
                text = "Display Orthographic";
                scatterGraph.orthoProjection = false
                // Orthographic projection disables shadows, so we need to switch them back on
                scatterGraph.shadowQuality = AbstractGraph3D.ShadowQualityLow
            } else {
                text = "Display Perspective";
                scatterGraph.orthoProjection = true
            }
        }
    }
    //! [8]

    NewButton {
        id: exitButton
        width: parent.width / 3
        text: "Quit"
        anchors.left: orthoToggle.right
        onClicked: Qt.quit(0);
    }
}
