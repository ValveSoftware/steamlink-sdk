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

Item {
    id: mainView
    width: 1280
    height: 1024

    property int sampleColumns: sampleSlider.value
    property int sampleRows: sampleColumns / 2
    property int sampleCache: 24

    onSampleRowsChanged: {
        surfaceSeries.selectedPoint = surfaceSeries.invalidSelectionPosition
        generateData()
    }

    Item {
        id: dataView
        anchors.bottom: parent.bottom
        width: parent.width
        height: parent.height - buttonLayout.height

        Surface3D {
            id: surfaceGraph

            width: dataView.width
            height: dataView.height
            shadowQuality: AbstractGraph3D.ShadowQualityNone
            selectionMode: AbstractGraph3D.SelectionSlice | AbstractGraph3D.SelectionItemAndRow
            //! [5]
            renderingMode: AbstractGraph3D.RenderDirectToBackground
            //! [5]

            axisX.labelFormat: "%d ms"
            axisY.labelFormat: "%d W"
            axisZ.labelFormat: "%d mV"
            axisX.min: 0
            axisY.min: 0
            axisZ.min: 0
            axisX.max: 1000
            axisY.max: 100
            axisZ.max: 800
            axisX.segmentCount: 4
            axisY.segmentCount: 4
            axisZ.segmentCount: 4
            measureFps: true

            onCurrentFpsChanged: {
                if (fps > 10)
                    fpsText.text = "FPS: " + Math.round(surfaceGraph.currentFps)
                else
                    fpsText.text = "FPS: " + Math.round(surfaceGraph.currentFps * 10.0) / 10.0
            }

            //! [0]
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
            //! [0]

            //! [2]
            Component.onCompleted: mainView.generateData()
            //! [2]
        }
    }

    //! [3]
    Timer {
        id: refreshTimer
        interval: 1000 / frequencySlider.value
        running: true
        repeat: true
        onTriggered: dataSource.update(surfaceSeries)
    }
    //! [3]

    Rectangle {
        width: parent.width
        height: flatShadingToggle.height * 2
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
                Layout.minimumHeight: flatShadingToggle.height
                spacing: 0

                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: samplesText.implicitWidth + 120
                    Layout.maximumWidth: samplesText.implicitWidth + 120
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                    border.color: "gray"
                    border.width: 1
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: parent.border.width + 1

                        Slider {
                            id: sampleSlider
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                            Layout.minimumWidth: 80
                            minimumValue: mainView.sampleCache * 2
                            maximumValue: minimumValue * 10
                            stepSize: mainView.sampleCache
                            updateValueWhileDragging: false
                            Component.onCompleted: value = minimumValue * 2
                        }

                        Rectangle {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.minimumWidth: samplesText.implicitWidth + 10
                            Layout.maximumWidth: samplesText.implicitWidth + 10
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                            Text {
                                id: samplesText
                                text: "Samples: " + (mainView.sampleRows * mainView.sampleColumns)
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: frequencyText.implicitWidth + 120
                    Layout.maximumWidth: frequencyText.implicitWidth + 120
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                    border.color: "gray"
                    border.width: 1
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: parent.border.width + 1

                        Slider {
                            id: frequencySlider
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                            Layout.minimumWidth: 80
                            minimumValue: 2
                            maximumValue: 60
                            stepSize: 2
                            updateValueWhileDragging: true
                            value: 30
                        }

                        Rectangle {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.minimumWidth: frequencyText.implicitWidth + 10
                            Layout.maximumWidth: frequencyText.implicitWidth + 10
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                            Text {
                                id: frequencyText
                                text: "Freq: " + frequencySlider.value + " Hz"
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: fpsText.implicitWidth + 10
                    Layout.maximumWidth: fpsText.implicitWidth + 10
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

                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: selectionText.implicitWidth + 10
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                    border.color: "gray"
                    border.width: 1
                    radius: 4

                    Text {
                        id: selectionText
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        text: "No selection"
                    }
                }
            }

            RowLayout {
                id: buttonLayout
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: flatShadingToggle.height
                anchors.bottom: parent.bottom
                spacing: 0

                NewButton {
                    id: flatShadingToggle
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    text: surfaceSeries.flatShadingSupported ? "Show Flat" : "Flat not supported"
                    enabled: surfaceSeries.flatShadingSupported

                    onClicked: {
                        if (surfaceSeries.flatShadingEnabled === true) {
                            surfaceSeries.flatShadingEnabled = false;
                            text = "Show Flat"
                        } else {
                            surfaceSeries.flatShadingEnabled = true;
                            text = "Show Smooth"
                        }
                    }
                }

                NewButton {
                    id: surfaceGridToggle
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    text: "Show Surface Grid"

                    onClicked: {
                        if (surfaceSeries.drawMode & Surface3DSeries.DrawWireframe) {
                            surfaceSeries.drawMode &= ~Surface3DSeries.DrawWireframe;
                            text = "Show Surface Grid"
                        } else {
                            surfaceSeries.drawMode |= Surface3DSeries.DrawWireframe;
                            text = "Hide Surface Grid"
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

    //! [4]
    function generateData() {
        dataSource.generateData(mainView.sampleCache, mainView.sampleRows,
                                mainView.sampleColumns, surfaceGraph.axisX.min,
                                surfaceGraph.axisX.max, surfaceGraph.axisY.min,
                                surfaceGraph.axisY.max, surfaceGraph.axisZ.min,
                                surfaceGraph.axisZ.max)
    }
    //! [4]
}
