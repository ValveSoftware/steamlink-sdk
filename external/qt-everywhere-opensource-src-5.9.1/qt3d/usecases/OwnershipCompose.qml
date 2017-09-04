/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

import QtQuick 2.1 as QQ2
import Qt3D 2.0

Node {
    // A Node contains any type of Node or QObject
    id : root
    objectName : "root"


    // Components defined in a node can be referenced by child nodes/entities

    Material { // This material is declared in a Node so that it can be referenced by
        // multiple Entity elements
        id : common_blue_material
        diffuseColor : "#0022dd"
        specularColor : "#0000ff";
    }

    Mesh {
        id : common_ball_mesh
        source : "my_ball_mesh.obj"
    }


    // Entity can be composed of components and child entities
    // that inherit its attributes (transformations, framegraph rendering)
    Entity {
        id : sceneRoot
        objectName : sceneRoot

        FrameGraph {
            SampleFrameGraphConfiguration {
                viewportRect : Qt.rect(0,0,1,1)
                camera : camera1 // Problem here, we cannot reference an Entity
            }
        }

        // Cameras are Entity composed of an Optic component called Camera
        // Maybe Camera should be an Entity subclass
        Entity {
            id : camera1
            objectName : "camera1"

            Transform {
                LookAt {}
                Translate {}
                Scale {}
                Rotate {}
            }

            Camera { // Maybe this should be renamed to optics
                projectionType: Camera.PerspectiveProjection
                fieldOfView: 22.5
                aspectRatio: 1920.0 / 1080.0
            }
        }

        Entity {
            id : testMesh
            objectName : "testMesh"

            property Mesh mesh : common_ball_mesh
            property Material material : common_blue_material

            Node {
                id : testMeshMaterialsNode
                Material {id : testMeshMaterialMetal}
                Material {id : testMeshMaterialWood}
                Transform {
                    id : perpetualRotation
                    Rotate {id : y_pertual_Rotation; axis : Qt.vector3d(0,1,0); angle : 0}
                    QQ2.Timer {
                        running : true
                        interval : 1000 / 24
                        repeat: true
                        onTriggered: y_pertual_Rotation.angle += 0.1
                    }
                }

                Entity {
                    id : woodBall
                    property Mesh mesh : common_ball_mesh
                    property Material mat : testMeshMaterialWood
                    property Transform trans : perpetualRotation
                } // woodBall

                Entity {
                    id : glossyCylinder
                    property Transform trans : perpetualRotation
                    Mesh {
                        id : cylinderMesh
                        source : "my_cylinder_mesh.obj"
                        Material {
                            id : glossyMaterial
                            diffuseColor : "#ffffff"
                            specularColor : "#ffffff"
                        }
                    }
                } // glossyCylinder

                Entity {
                    id : metalBall
                    Transform {
                        Translate {dy : 5; dx : 0; dz : -8}
                    }
                    Mesh {
                        property Material mat : testMeshMaterialMetal
                        source : "my_ball_mesh.obj"
                    }
                } // metalBall

            } // testMeshMaterialsNode

        } // testMesh

    } // sceneRoot

} // root
