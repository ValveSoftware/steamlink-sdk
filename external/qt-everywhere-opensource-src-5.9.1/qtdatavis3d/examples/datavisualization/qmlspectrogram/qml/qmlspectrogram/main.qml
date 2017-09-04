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
import QtQuick.Layouts 1.0
import QtQuick.Window 2.1
import QtDataVisualization 1.2
import "."

Window {
    id: mainview
    title: "Qt Quick 2 Spectrogram Example"
    visible: true
    width: 1024
    height: 768
    color: surfaceGraph.theme.windowColor

    Data {
        id: surfaceData
    }

    Item {
        id: surfaceView
        width: mainview.width
        height: mainview.height
        anchors.top: mainview.top
        anchors.left: mainview.left

        ColorGradient {
            id: surfaceGradient
            ColorGradientStop { position: 0.0; color: "black" }
            ColorGradientStop { position: 0.2; color: "red" }
            ColorGradientStop { position: 0.5; color: "blue" }
            ColorGradientStop { position: 0.8; color: "yellow" }
            ColorGradientStop { position: 1.0; color: "white" }
        }

        ValueAxis3D {
            id: xAxis
            segmentCount: 8
            labelFormat: "%i\u00B0"
            title: "Angle"
            titleVisible: true
            titleFixed: false
        }

        ValueAxis3D {
            id: yAxis
            segmentCount: 8
            labelFormat: "%i \%"
            title: "Value"
            titleVisible: true
            labelAutoRotation: 0
            titleFixed: false
        }

        ValueAxis3D {
            id: zAxis
            segmentCount: 5
            labelFormat: "%i nm"
            title: "Radius"
            titleVisible: true
            titleFixed: false
        }

        Theme3D {
            id: customTheme
            type: Theme3D.ThemeQt
            // Don't show specular spotlight as we don't want it to distort the colors
            lightStrength: 0.0
            ambientLightStrength: 1.0
            backgroundEnabled: false
            gridLineColor: "#AAAAAA"
            windowColor: "#EEEEEE"
        }


        //! [5]
        TouchInputHandler3D {
            id: customInputHandler
            rotationEnabled: false
        }
        //! [5]

        //! [0]
        //! [7]
        Surface3D {
            //! [7]
            id: surfaceGraph
            width: surfaceView.width
            height: surfaceView.height

            shadowQuality: AbstractGraph3D.ShadowQualityNone
            selectionMode: AbstractGraph3D.SelectionSlice | AbstractGraph3D.SelectionItemAndColumn
            axisX: xAxis
            axisY: yAxis
            axisZ: zAxis

            theme: customTheme
            //! [6]
            inputHandler: customInputHandler
            //! [6]

            // Remove the perspective and view the graph from top down to achieve 2D effect
            //! [1]
            orthoProjection: true
            scene.activeCamera.cameraPreset: Camera3D.CameraPresetDirectlyAbove
            //! [1]

            //! [2]
            flipHorizontalGrid: true
            //! [2]

            //! [4]
            radialLabelOffset: 0.01
            //! [4]

            horizontalAspectRatio: 1
            scene.activeCamera.zoomLevel: 85

            Surface3DSeries {
                id: surfaceSeries
                flatShadingEnabled: false
                drawMode: Surface3DSeries.DrawSurface
                baseGradient: surfaceGradient
                colorStyle: Theme3D.ColorStyleRangeGradient
                itemLabelFormat: "(@xLabel, @zLabel): @yLabel"

                ItemModelSurfaceDataProxy {
                    itemModel: surfaceData.model
                    rowRole: "radius"
                    columnRole: "angle"
                    yPosRole: "value"
                }
            }
        }
        //! [0]
    }

    RowLayout {
        id: buttonLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        opacity: 0.5

        //! [3]
        NewButton {
            id: polarToggle
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: "Switch to polar"
            onClicked: {
                if (surfaceGraph.polar === false) {
                    surfaceGraph.polar = true
                    text = "Switch to cartesian"
                } else {
                    surfaceGraph.polar = false
                    text = "Switch to polar"
                }
            }
        }
        //! [3]

        NewButton {
            id: orthoToggle
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: "Switch to perspective"
            onClicked: {
                if (surfaceGraph.orthoProjection === true) {
                    surfaceGraph.orthoProjection = false;
                    xAxis.labelAutoRotation = 30
                    yAxis.labelAutoRotation = 30
                    zAxis.labelAutoRotation = 30
                    customInputHandler.rotationEnabled = true
                    text = "Switch to orthographic"
                } else {
                    surfaceGraph.orthoProjection = true;
                    surfaceGraph.scene.activeCamera.cameraPreset = Camera3D.CameraPresetDirectlyAbove
                    surfaceSeries.drawMode &= ~Surface3DSeries.DrawWireframe;
                    xAxis.labelAutoRotation = 0
                    yAxis.labelAutoRotation = 0
                    zAxis.labelAutoRotation = 0
                    customInputHandler.rotationEnabled = false
                    text = "Switch to perspective"
                }
            }
        }

        NewButton {
            id: flipGridToggle
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: "Toggle axis grid on top"
            onClicked: {
                onClicked: {
                    if (surfaceGraph.flipHorizontalGrid === true) {
                        surfaceGraph.flipHorizontalGrid = false;
                    } else {
                        surfaceGraph.flipHorizontalGrid = true;
                    }
                }
            }
        }

        NewButton {
            id: labelOffsetToggle
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: "Toggle radial label position"
            visible: surfaceGraph.polar
            onClicked: {
                if (surfaceGraph.radialLabelOffset >= 1.0) {
                    surfaceGraph.radialLabelOffset = 0.01
                } else {
                    surfaceGraph.radialLabelOffset = 1.0
                }
            }
        }

        NewButton {
            id: surfaceGridToggle
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: "Toggle surface grid"
            visible: !surfaceGraph.orthoProjection
            onClicked: {
                if (surfaceSeries.drawMode & Surface3DSeries.DrawWireframe) {
                    surfaceSeries.drawMode &= ~Surface3DSeries.DrawWireframe;
                } else {
                    surfaceSeries.drawMode |= Surface3DSeries.DrawWireframe;
                }
            }
        }

    }

    Rectangle {
        id: legend
        anchors.margins: 20
        anchors.bottom: parent.bottom
        anchors.top: buttonLayout.bottom
        anchors.right: parent.right
        border.color: "black"
        border.width: 1
        width: 50
        rotation: 180
        gradient: Gradient {
            GradientStop { position: 0.0; color: "black" }
            GradientStop { position: 0.2; color: "red" }
            GradientStop { position: 0.5; color: "blue" }
            GradientStop { position: 0.8; color: "yellow" }
            GradientStop { position: 1.0; color: "white" }
        }
    }

    Text {
        anchors.verticalCenter: legend.bottom
        anchors.right: legend.left
        anchors.margins: 2
        text: surfaceGraph.axisY.min  + "%"
    }

    Text {
        anchors.verticalCenter: legend.verticalCenter
        anchors.right: legend.left
        anchors.margins: 2
        text: (surfaceGraph.axisY.max + surfaceGraph.axisY.min) / 2  + "%"
    }

    Text {
        anchors.verticalCenter: legend.top
        anchors.right: legend.left
        anchors.margins: 2
        text: surfaceGraph.axisY.max + "%"
    }
}
