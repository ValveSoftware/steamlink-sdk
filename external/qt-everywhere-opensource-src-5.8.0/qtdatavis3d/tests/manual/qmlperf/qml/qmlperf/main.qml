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
import QtQuick.Controls 1.0
import QtDataVisualization 1.1
import "."

Rectangle {
    id: mainview
    width: 1280
    height: 1024

    property var itemCount: 1000.0
    property var addItems: 500.0

    Button {
        id: changeButton
        width: parent.width / 7
        height: 50
        anchors.left: parent.left
        enabled: true
        text: "Change"
        onClicked: {
            console.log("changeButton clicked");
            if (graphView.state == "meshsphere") {
                graphView.state = "meshcube"
            } else if (graphView.state == "meshcube") {
                graphView.state = "meshpyramid"
            } else if (graphView.state == "meshpyramid") {
                graphView.state = "meshpoint"
            } else if (graphView.state == "meshpoint") {
                graphView.state = "meshsphere"
            }
        }
    }

    Text {
        id: fpsText
        text: "Reading"
        width: (parent.width / 7) * 3
        height: 50
        anchors.left: changeButton.right
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    Button {
        id: optimization
        width: parent.width / 7
        height: 50
        anchors.left: fpsText.right
        enabled: true
        text: scatterPlot.optimizationHints === AbstractGraph3D.OptimizationDefault ? "To Static" : "To Default"
        onClicked: {
            console.log("Optimization");
            if (scatterPlot.optimizationHints === AbstractGraph3D.OptimizationDefault) {
                scatterPlot.optimizationHints = AbstractGraph3D.OptimizationStatic;
                optimization.text = "To Default";
            } else {
                scatterPlot.optimizationHints = AbstractGraph3D.OptimizationDefault;
                optimization.text = "To Static";
            }
        }
    }

    Button {
        id: itemAdd
        width: parent.width / 7
        height: 50
        anchors.left: optimization.right
        enabled: true
        text: "Add"
        onClicked: {
            itemCount = itemCount + addItems;
            dataGenerator.add(scatterSeries, addItems);
        }
    }

    Button {
        id: writeLine
        width: parent.width / 7
        height: 50
        anchors.left: itemAdd.right
        enabled: true
        text: "Write"
        onClicked: {
            dataGenerator.writeLine(itemCount, scatterPlot.currentFps.toFixed(1));
        }
    }

    Item {
        id: graphView
        width: mainview.width
        height: mainview.height
        anchors.top: changeButton.bottom
        anchors.left: mainview.left
        state: "meshsphere"

        Scatter3D {
            id: scatterPlot
            width: graphView.width
            height: graphView.height
            shadowQuality: AbstractGraph3D.ShadowQualityNone
            optimizationHints: AbstractGraph3D.OptimizationDefault
            scene.activeCamera.yRotation: 45.0
            measureFps: true
            onCurrentFpsChanged: {
                fpsText.text = itemCount + " : " + scatterPlot.currentFps.toFixed(1);
            }

//            theme: Theme3D {
//                type: Theme3D.ThemeRetro
//                colorStyle: Theme3D.ColorStyleRangeGradient
//                baseGradients: customGradient

//                ColorGradient {
//                    id: customGradient
//                    ColorGradientStop { position: 1.0; color: "red" }
//                    ColorGradientStop { position: 0.0; color: "blue" }
//                }
//            }

            Scatter3DSeries {
                id: scatterSeries
                mesh: Abstract3DSeries.MeshSphere
            }

            Component.onCompleted: dataGenerator.generateData(scatterSeries, itemCount);
        }

        states: [
            State {
                name: "meshsphere"
                StateChangeScript {
                    name: "doSphere"
                    script: {
                        console.log("Do the sphere");
                        scatterSeries.mesh = Abstract3DSeries.MeshSphere;
                    }
                }
            },
            State {
                name: "meshcube"
                StateChangeScript {
                    name: "doCube"
                    script: {
                        console.log("Do the cube");
                        scatterSeries.mesh = Abstract3DSeries.MeshCube;
                    }
                }
            },
            State {
                name: "meshpyramid"
                StateChangeScript {
                    name: "doPyramid"
                    script: {
                        console.log("Do the pyramid");
                        scatterSeries.mesh = Abstract3DSeries.MeshPyramid;
                    }
                }
            },
            State {
                name: "meshpoint"
                StateChangeScript {
                    name: "doPoint"
                    script: {
                        console.log("Do the point");
                        scatterSeries.mesh = Abstract3DSeries.MeshPoint;
                    }
                }
            }
        ]
    }
}
