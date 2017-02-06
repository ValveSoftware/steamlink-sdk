/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtCanvas3D 1.1
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0

import "commandthroughput.js" as GLCode

Item {
    id: mainview
    width: 1280
    height: 768
    visible: true

    Canvas3D {
        id: canvas3d
        anchors.fill: parent
        renderOnDemand: true
        renderTarget: Canvas3D.RenderTargetBackground
        property double xRotSlider: 0
        property double yRotSlider: 0
        property double zRotSlider: 0
        property double xRotAnim: 0
        property double yRotAnim: 0
        property double zRotAnim: 0
        property int itemCount: 0
        property int maxCount: 5000
        property int frameTime: 0
        property int frameSetupTime: 0

        // Emitted when one time initializations should happen
        onInitializeGL: {
            GLCode.initializeGL(canvas3d);
        }

        // Emitted each time Canvas3D is ready for a new frame
        onPaintGL: {
            GLCode.paintGL(canvas3d);
        }

        // If width or height or pixel ratio changes
        // we need to react to that in the rendering code
        onWidthChanged: {
            GLCode.onCanvasResize(canvas3d);
        }

        onHeightChanged: {
            GLCode.onCanvasResize(canvas3d);
        }

        onDevicePixelRatioChanged: {
            GLCode.onCanvasResize(canvas3d);
        }
    }

    Timer {
        interval: 500
        repeat: true
        onTriggered: {
            canvas3d.frameTime = canvas3d.frameTimeMs();
            canvas3d.frameSetupTime = canvas3d.frameSetupTimeMs();
        }
        Component.onCompleted: start();
    }

    RowLayout {
        id: controlLayout
        spacing: 5
        x: 0
        y: parent.height-100
        width: parent.width
        height: 100
        visible: true

        Label {
            id: fpsLabel
            Layout.alignment: Qt.AlignRight
            Layout.fillWidth: false
            Layout.preferredWidth : 200
            text: "Fps: " + canvas3d.fps + " GL:" + canvas3d.frameTime + " onPaintGL:" + canvas3d.frameSetupTime
            color: "#FFFFFF"
        }

        Button {
            id: demandButton
            Layout.fillWidth: false
            Layout.preferredWidth : 100
            text: canvas3d.renderOnDemand ? "On-demand" : "Continuous"
            checkable: false
            onClicked: canvas3d.renderOnDemand = !canvas3d.renderOnDemand
        }

        Label {
            id: rotLabel
            Layout.alignment: Qt.AlignRight
            Layout.fillWidth: false
            text: "Rotation:"
            color: "#FFFFFF"
        }

        Slider {
            id: rotSlider
            Layout.alignment: Qt.AlignLeft
            Layout.preferredWidth : 300
            Layout.fillWidth: true
            minimumValue: 0
            maximumValue: 360
            onValueChanged: {
                canvas3d.xRotSlider = value;
                canvas3d.yRotSlider = value;
                canvas3d.zRotSlider = value;
                canvas3d.requestRender();
            }
        }

        Label {
            id: countLabel
            Layout.alignment: Qt.AlignRight
            Layout.fillWidth: false
            text: "Count: " + canvas3d.itemCount
            color: "#FFFFFF"
        }

        Slider {
            id: countSlider
            Layout.alignment: Qt.AlignLeft
            Layout.preferredWidth : 300
            Layout.fillWidth: true
            value: 48
            minimumValue: 1
            maximumValue: canvas3d.maxCount

            onValueChanged: {
                canvas3d.itemCount = value;
                canvas3d.requestRender();
            }
        }
    }
}
