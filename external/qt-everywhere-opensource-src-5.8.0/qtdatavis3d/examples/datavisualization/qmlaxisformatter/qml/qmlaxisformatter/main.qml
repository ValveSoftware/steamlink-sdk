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
import QtDataVisualization 1.1
import CustomFormatter 1.0
import "."

Rectangle {
    id: mainView
    width: 1280
    height: 1024

    Data {
        id: seriesData
    }

    Theme3D {
        id: themeIsabelle
        type: Theme3D.ThemePrimaryColors
        font.family: "Lucida Handwriting"
        font.pointSize: 40
    }

    //! [1]
    ValueAxis3D {
        id: dateAxis
        formatter: CustomFormatter {
            originDate:  "2014-01-01"
            selectionFormat: "yyyy-MM-dd HH:mm:ss"
        }
        subSegmentCount: 2
        labelFormat: "yyyy-MM-dd"
        min: 0
        max: 14
    }
    //! [1]

    //! [2]
    ValueAxis3D {
        id: logAxis
        formatter: LogValueAxis3DFormatter {
            id: logAxisFormatter
            base: 10
            autoSubGrid: true
            showEdgeLabels: true
        }
        labelFormat: "%.2f"
    }
    //! [2]

    ValueAxis3D {
        id: linearAxis
        labelFormat: "%.2f"
        min: 0
        max: 500
    }

    //! [0]
    ValueAxis3D {
        id: valueAxis
        segmentCount: 5
        subSegmentCount: 2
        labelFormat: "%.2f"
        min: 0
        max: 10
    }
    //! [0]

    Item {
        id: dataView
        anchors.bottom: parent.bottom
        width: parent.width
        height: parent.height - buttonLayout.height

        Scatter3D {
            id: scatterGraph
            width: dataView.width
            height: dataView.height
            theme: themeIsabelle
            shadowQuality: AbstractGraph3D.ShadowQualitySoftLow
            scene.activeCamera.cameraPreset: Camera3D.CameraPresetIsometricRight
            //! [3]
            axisZ: valueAxis
            axisY: logAxis
            axisX: dateAxis
            //! [3]

            Scatter3DSeries {
                id: scatterSeries
                itemLabelFormat: "@xLabel - (@yLabel, @zLabel)"
                meshSmooth: true
                ItemModelScatterDataProxy {
                    itemModel: seriesData.model
                    xPosRole: "xPos"
                    yPosRole: "yPos"
                    zPosRole: "zPos"
                }
            }
        }
    }

    RowLayout {
        id: buttonLayout
        Layout.minimumHeight: exitButton.height
        width: parent.width
        anchors.left: parent.left
        spacing: 0

        NewButton {
            id: yAxisBaseChange
            Layout.fillHeight: true
            Layout.fillWidth: true
            state: "enabled"
            onClicked: {
                if (logAxisFormatter.base === 10)
                    logAxisFormatter.base = 0
                else if (logAxisFormatter.base === 2)
                    logAxisFormatter.base = 10
                else
                    logAxisFormatter.base = 2
            }
            states: [
                State {
                    name: "enabled"
                    PropertyChanges {
                        target: yAxisBaseChange
                        text: "Y-axis log base: " + logAxisFormatter.base
                        enabled: true
                    }
                },
                State {
                    name: "disabled"
                    PropertyChanges {
                        target: yAxisBaseChange
                        text: "Y-axis linear"
                        enabled: false
                    }
                }
            ]
        }

        NewButton {
            id: yAxisToggle
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Toggle Y-axis"
            onClicked: {
                if (scatterGraph.axisY === linearAxis) {
                    scatterGraph.axisY = logAxis
                    yAxisBaseChange.state = "enabled"
                } else {
                    scatterGraph.axisY = linearAxis
                    yAxisBaseChange.state = "disabled"
                }
            }
        }

        NewButton {
            id: exitButton
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Quit"
            onClicked: Qt.quit(0);
        }
    }
}
