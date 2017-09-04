/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0 as Quick
import QtQuick.Layouts 1.3
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.9
import Qt3D.Animation 2.9
import QtQuick.Scene3D 2.0


Quick.Item {

    width: 1250
    height: 900

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 0

        Quick.Rectangle {
            id: background
            width: 1250
            height: 700

            color: "red"

            Scene3D {

                anchors.fill: parent
                multisample: true
                focus: true

                aspects: ["input", "logic"]

                Entity {
                    components: [
                        RenderSettings {
                            activeFrameGraph: ForwardRenderer {
                                clearColor: Qt.rgba(0, 0.5, 1, 1)
                                camera: camera
                            }
                        },
                        // Event Source will be set by the Qt3DQuickWindow
                        InputSettings { }
                    ]

                    Camera {
                        id: camera
                        projectionType: CameraLens.PerspectiveProjection
                        fieldOfView: 45
                        aspectRatio: 16/9
                        nearPlane : 0.1
                        farPlane : 1000.0
                        position: Qt.vector3d( 0.0, 1.0, 10.0 )
                        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
                        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
                    }

                    FirstPersonCameraController {
                        camera: camera
                    }

                    Entity {
                        id: scene2
                        components: [
                            Transform {
                                scale: 0.2
                                translation: Qt.vector3d(0.0, 0.0, 0.0)
                            },
                            SceneLoader {
                                id: animEntity
                                source: "qrc:/assets/gears.dae"

                                AnimationController {
                                    id: animator
                                    property bool initialized : false
                                    onPositionChanged: {
                                        slider.value = position
                                    }
                                }
                                Quick.PropertyAnimation {
                                    id: animPosition
                                    target: animator
                                    property: "position"
                                    loops: Quick.Animation.Infinite
                                    from: 0.0
                                }
                                onStatusChanged: {
                                    console.log(status)
                                    if (status === SceneLoader.Ready) {

                                        animator.entity = null
                                        animator.entity = scene2
                                        animator.activeAnimationGroup = 0
                                        var group = animator.getGroup(0)
                                        animPosition.to = group.duration
                                        animPosition.duration = group.duration * 1000
                                        slider.maximumValue = group.duration

                                        var animList = []
                                        var groups = animator.animationGroups
                                        for (var i = 0; i < groups.length; i++) {
                                            if (groups[i].name === "")
                                                groups[i].name = "UnnamedAnim" + i;
                                            animList.push(groups[i].name)
                                        }
                                        comboBox.model = animList

                                        animator.initialized = true
                                        comboBox.currentIndex = 0
                                    }
                                }
                            }
                        ]
                    }
                }
            }
        }

        ComboBox {
            id: animationSelector
            anchors.topMargin: 10
            anchors.top: background.bottom
            implicitWidth: 400
            model: [ "Gears", "Blend Shape" ]
            onCurrentIndexChanged: {

                if (animPosition.running) {
                    animPosition.stop()
                    animator.position = 0
                    slider.value = 0
                }

                if (currentText === "Gears" && animEntity.source !== "qrc:/assets/gears.dae")
                    animEntity.source = "qrc:/assets/gears.dae"
                else if (currentText === "Blend Shape"
                         && animEntity.source !== "qrc:/assets/blendshapeanimation.dae") {
                    animEntity.source = "qrc:/assets/blendshapeanimation.dae"
                }
            }
        }

        ComboBox {
            id: comboBox
            anchors.topMargin: 10
            anchors.top: animationSelector.bottom
            implicitWidth: 400
            onCurrentIndexChanged: {
                if (animator.initialized) {
                    animator.activeAnimationGroup = currentIndex
                    var group = animator.getGroup(currentIndex)
                    animPosition.to = group.duration
                    animPosition.duration = group.duration * 1000
                    slider.maximumValue = group.duration
                }
            }
        }

        RowLayout {
            anchors.left: parent.left

            Button {
                text: "play"
                onClicked: {
                    if (!animPosition.running || animPosition.paused)
                        animPosition.start()
                }
            }
            Button {
                text: "stop"
                onClicked: {
                    if (animPosition.running) {
                        animPosition.stop()
                        animator.position = 0
                        slider.value = 0
                    }
                }
            }
            Button {
                text: "pause"
                onClicked: {
                    if (!animPosition.paused && animPosition.running)
                        animPosition.pause()
                }
            }
        }
        Slider {
            id: slider
            anchors.bottomMargin: 10
            anchors.left: parent.left
            anchors.right: parent.right
            value: 0.0
            onValueChanged: {
                if (pressed) {
                    animPosition.stop()
                    animator.position = value
                }
            }
        }
    }
}
