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

import "framebuffer.js" as GLCode

Rectangle {
    id: mainView
    anchors.fill: parent
    visible: true
    color: "#f2f2f2"

    property alias canvas3d: canvas3d
    property string canvasName: ""
    property var previousParent: null

    onParentChanged: {
        if (previousParent && previousParent.handleParentChange)
            previousParent.handleParentChange()
        previousParent = parent
    }

    Canvas3D {
        id: canvas3d
        anchors.fill: parent
        property double xRotSlider: 0
        property double yRotSlider: 0
        property double zRotSlider: 0
        property double xRotAnim: 0
        property double yRotAnim: 0
        property double zRotAnim: 0
        property bool isRunning: true

        // Emitted when one time initializations should happen
        onInitializeGL: {
            GLCode.initializeGL(canvas3d);
        }

        // Emitted each time Canvas3D is ready for a new frame
        onPaintGL: {
            if (canvas3d.renderTarget === Canvas3D.RenderTargetOffscreenBuffer)
                GLCode.paintGL(canvas3d, true);
            else
                GLCode.paintGL(canvas3d, false);
        }

        onResizeGL: {
            GLCode.resizeGL(canvas3d);
        }

        onContextLost: {
            console.log("Context lost on: ", mainView.canvasName)
            GLCode.handleContextLost();
        }

        onContextRestored: {
            console.log("Context restored on: ", mainView.canvasName)
        }

        Keys.onSpacePressed: {
            canvas3d.isRunning = !canvas3d.isRunning
            if (canvas3d.isRunning) {
                objAnimationX.pause();
                objAnimationY.pause();
                objAnimationZ.pause();
            } else {
                objAnimationX.resume();
                objAnimationY.resume();
                objAnimationZ.resume();
            }
        }

        SequentialAnimation {
            id: objAnimationX
            loops: Animation.Infinite
            running: true
            NumberAnimation {
                target: canvas3d
                property: "xRotAnim"
                from: 0.0
                to: 120.0
                duration: 7000
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: canvas3d
                property: "xRotAnim"
                from: 120.0
                to: 0.0
                duration: 7000
                easing.type: Easing.InOutQuad
            }
        }

        SequentialAnimation {
            id: objAnimationY
            loops: Animation.Infinite
            running: true
            NumberAnimation {
                target: canvas3d
                property: "yRotAnim"
                from: 0.0
                to: 240.0
                duration: 5000
                easing.type: Easing.InOutCubic
            }
            NumberAnimation {
                target: canvas3d
                property: "yRotAnim"
                from: 240.0
                to: 0.0
                duration: 5000
                easing.type: Easing.InOutCubic
            }
        }

        SequentialAnimation {
            id: objAnimationZ
            loops: Animation.Infinite
            running: true
            NumberAnimation {
                target: canvas3d
                property: "zRotAnim"
                from: -100.0
                to: 100.0
                duration: 3000
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                target: canvas3d
                property: "zRotAnim"
                from: 100.0
                to: -100.0
                duration: 3000
                easing.type: Easing.InOutSine
            }
        }
    }

    RowLayout {
        id: controlLayout
        spacing: 5
        x: 12
        y: parent.height - 100
        width: parent.width - (x * 2)
        height: 100
        visible: true

        Label {
            id: xRotLabel
            Layout.alignment: Qt.AlignRight
            Layout.fillWidth: false
            text: "X-axis:"
        }

        Slider {
            id: xSlider
            Layout.alignment: Qt.AlignLeft
            Layout.fillWidth: true
            minimumValue: 0;
            maximumValue: 360;
            onValueChanged: canvas3d.xRotSlider = value;
        }

        Label {
            id: yRotLabel
            Layout.alignment: Qt.AlignRight
            Layout.fillWidth: false
            text: "Y-axis:"
        }

        Slider {
            id: ySlider
            Layout.alignment: Qt.AlignLeft
            Layout.fillWidth: true
            minimumValue: 0;
            maximumValue: 360;
            onValueChanged: canvas3d.yRotSlider = value;
        }

        Label {
            id: zRotLabel
            Layout.alignment: Qt.AlignRight
            Layout.fillWidth: false
            text: "Z-axis:"
        }

        Slider {
            id: zSlider
            Layout.alignment: Qt.AlignLeft
            Layout.fillWidth: true
            minimumValue: 0;
            maximumValue: 360;
            onValueChanged: canvas3d.zRotSlider = value;
        }
    }
}
