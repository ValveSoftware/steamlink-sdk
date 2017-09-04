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
import QtDataVisualization 1.2
import "."

Item {
    id: mainView
    width: 1280
    height: 1024

    Item {
        id: dataView
        anchors.bottom: parent.bottom
        width: parent.width
        height: parent.height - buttonLayout.height

        Surface3D {
            id: surfaceGraph

            width: dataView.width
            height: dataView.height
            orthoProjection: true
            //measureFps: true

            onCurrentFpsChanged: {
                if (fps > 10)
                    fpsText.text = "FPS: " + Math.round(surfaceGraph.currentFps)
                else
                    fpsText.text = "FPS: " + Math.round(surfaceGraph.currentFps * 10.0) / 10.0
            }

            Surface3DSeries {
                id: surfaceSeries
                drawMode: Surface3DSeries.DrawSurface;
                flatShadingEnabled: false;
                meshSmooth: true
                itemLabelFormat: "@xLabel, @zLabel: @yLabel"
                itemLabelVisible: false

                onItemLabelChanged: {
                    if (surfaceSeries.selectedPoint === surfaceSeries.invalidSelectionPosition)
                        selectionText.text = "No selection"
                    else
                        selectionText.text = surfaceSeries.itemLabel
                }
            }

            Component.onCompleted: {
                mainView.createVolume();
            }
        }
    }

    Rectangle {
        width: parent.width
        height: 50
        anchors.left: parent.left
        anchors.top: parent.top
        color: surfaceGraph.theme.backgroundColor

        ColumnLayout {
            anchors.fill: parent
            RowLayout {
                id: sliderLayout
                anchors.top: parent.top
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: 150
                spacing: 0

                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: fpsText.implicitWidth + 10
                    Layout.maximumWidth: fpsText.implicitWidth + 10
                    Layout.minimumHeight: 50
                    Layout.maximumHeight: 50
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                    border.color: "gray"
                    border.width: 1
                    radius: 4

                    Text {
                        id: fpsText
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            RowLayout {
                id: buttonLayout
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: 50
                anchors.bottom: parent.bottom
                spacing: 0

                NewButton {
                    id: sliceButton
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    text: "Slice"

                    onClicked: {
                        if (volumeItem.sliceIndexZ == -1) {
                            volumeItem.sliceIndexZ = 128
                            volumeItem.drawSlices = true
                            volumeItem.drawSliceFrames = true
                        } else {
                            volumeItem.sliceIndexZ = -1
                            volumeItem.drawSlices = false
                            volumeItem.drawSliceFrames = false
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

    }

    Custom3DVolume {
        id: volumeItem
        alphaMultiplier: 0.3
        preserveOpacity: true
        useHighDefShader: false
    }

    function createVolume() {
        surfaceGraph.addCustomItem(volumeItem)
        dataSource.fillVolume(volumeItem)
    }
}
