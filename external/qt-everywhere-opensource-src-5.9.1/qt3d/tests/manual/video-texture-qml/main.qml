/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

import Qt3D.Core 2.0
import Qt3D.Render 2.9
import Qt3D.Input 2.0
import QtQuick 2.2 as QQ2
import QtQuick.Scene2D 2.9
import QtQuick.Window 2.0 as QW2
import Qt3D.Extras 2.0
import QtMultimedia 5.6 as QMM
import QtQuick.Dialogs 1.0

Entity {
    id: sceneRoot

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: _window.width / _window.height
        nearPlane : 0.1
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 0.0, 3.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    Scene2D {
        output: RenderTargetOutput {
            attachmentPoint: RenderTargetOutput.Color0
            texture: Texture2D {
                id: offscreenTexture
                width: 1024
                height: 1024
                format: Texture.RGBA8_UNorm
                generateMipMaps: false
                magnificationFilter: Texture.Linear
                minificationFilter: Texture.Linear
                wrapMode {
                    x: WrapMode.ClampToEdge
                    y: WrapMode.ClampToEdge
                }
            }
        }
        QQ2.Rectangle {
            width: 400
            height: 300
            color: "green"

            QMM.MediaPlayer {
                id: player
                autoPlay: false
                loops: QMM.MediaPlayer.Infinite
            }

            QMM.VideoOutput {
                id: videoOutput
                source: player
                anchors.fill: parent
            }
        }
    }

    FirstPersonCameraController {
        camera: camera
    }

    components: [
        RenderSettings {
            activeFrameGraph:
                ForwardRenderer {
                    camera: camera
                }
        },
        InputSettings {}
    ]

    CuboidMesh {
        id: mesh
    }

    Entity {
        id: entity

        property Transform transform: Transform {
            scale: 1
            translation: Qt.vector3d(0,0,0)
        }

        property Material material: PlaneMaterial {
            texture: offscreenTexture
        }

        components: [mesh, material, transform]
    }

    FileDialog {
        id: fileDialog
        title: "Please choose a video"
        folder: shortcuts.home
        onAccepted: {
            visible = false
            player.source = fileDialog.fileUrl
            player.play()
        }
        onRejected: {
            Qt.quit()
        }
        QQ2.Component.onCompleted: {
            visible = true
        }
    }
}
