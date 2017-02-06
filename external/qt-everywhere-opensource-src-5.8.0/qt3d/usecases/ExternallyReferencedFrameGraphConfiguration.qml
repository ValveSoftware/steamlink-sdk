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
    // Node can contains any type of Node or QObject
    id : root
    objectName : "root"


    // Entity can be composed of components and child entities
    // than inherit its attributes (transformations, framegraph rendering)
    Entity {
        id : sceneRoot
        objectName : sceneRoot


        property FrameGraph frameGraph : FrameGraph { // subclass of Component
            // activeFrameGraph: references the frameGraph
            // configuration
            activeFrameGraph : SampleFrameGraphConfiguration {
                camera : camera1
                viewportRect : Qt.rect(0, 0, 1, 1)
            }
        } // frameGraph

        // Cameras are Entity composed of an Optic component called Camera
        Entity {
            id : camera1
            objectName : "camera1"

            // Transform is a component
            // Contains transformation childComponents
            property Transform transform : Transform {
                LookAt {}
                Translate {}
                Scale {}
                Rotate {}
            }

            property Camera optics : Camera {
                projectionType: Camera.PerspectiveProjection
                fieldOfView: 22.5
                aspectRatio: 1920.0 / 1080.0
            }
        }

        Entity {
            id : testMesh
            objectName : "testMesh"

            property Mesh mesh : Mesh { // subclass of Component
                source : "test_mesh.obj"
            }

            property Material material : Material { // subclass of Component
                diffuseColor : "#00ff00"
                specularColor : "#00ff00"
            }
        }


    } // sceneRoot

} // root
