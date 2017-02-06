/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2 as QQ2
import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Entity {
    id: sceneRoot

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 16/9
        nearPlane : 0.1
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 0.0, 40.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    OrbitCameraController { camera: camera }

    components: [
        RenderSettings {
            activeFrameGraph: ForwardRenderer {
                clearColor: Qt.rgba(0.5, 0.5, 1, 1)
                camera: camera
            }
        },
        // Event Source will be set by the Qt3DQuickWindow
        InputSettings { }
    ]

    PerVertexColorMaterial {
        id: material
    }

    function buildVertexBufferData()
    {
        // Vertices
        var v0 = Qt.vector3d(-1.0, 0.0, -1.0)
        var v1 = Qt.vector3d(1.0, 0.0, -1.0)
        var v2 = Qt.vector3d(0.0, 1.0, 0.0)
        var v3 = Qt.vector3d(0.0, 0.0, 1.0)

        // Face Normals
        function normal(v0, v1, v2) {
            return v1.minus(v0).crossProduct(v2.minus(v0)).normalized();
        }
        var n023 = normal(v0, v2, v3)
        var n012 = normal(v0, v1, v2)
        var n310 = normal(v3, v1, v0)
        var n132 = normal(v1, v3, v2)

        // Vector normals
        var n0 = n023.plus(n012).plus(n310).normalized()
        var n1 = n132.plus(n012).plus(n310).normalized()
        var n2 = n132.plus(n012).plus(n023).normalized()
        var n3 = n132.plus(n310).plus(n023).normalized()

        // Colors
        var red = Qt.vector3d(1.0, 0.0, 0.0)
        var green = Qt.vector3d(0.0, 1.0, 0.0)
        var blue = Qt.vector3d(0.0, 0.0, 1.0)
        var white = Qt.vector3d(1.0, 1.0, 1.0)

        var vertices = [
                    v0, n0, red,
                    v1, n1, blue,
                    v2, n2, green,
                    v3, n3, white
                ]

        var vertexArray = new Float32Array(4 * (3 + 3 + 3));
        var i = 0;

        vertices.forEach(function(vec3) {
            vertexArray[i++] = vec3.x;
            vertexArray[i++] = vec3.y;
            vertexArray[i++] = vec3.z;
        });

        return vertexArray;
    }

    function buildIndexBufferData()
    {
        var indexArray = new Uint16Array(12);

        // Front
        indexArray[0] = 0;
        indexArray[1] = 1;
        indexArray[2] = 2;
        // Bottom
        indexArray[3] = 3;
        indexArray[4] = 1;
        indexArray[5] = 0;
        // Left
        indexArray[6] = 0;
        indexArray[7] = 2;
        indexArray[8] = 3;
        // Right
        indexArray[9] = 1;
        indexArray[10] = 3;
        indexArray[11] = 2;

        return indexArray;
    }

    function updateVertexColor()
    {
        var angle = colorBufferTimerUpdate.angle

        var c1 = Qt.vector3d(Math.abs(Math.cos(angle + Math.PI * 0.25)),
                             Math.abs(Math.sin(angle)),
                             Math.abs(Math.cos(angle)));
        var c2 = Qt.vector3d(Math.abs(Math.sin(angle)),
                             Math.abs(Math.cos(angle + Math.PI * 0.25)),
                             Math.abs(Math.sin(angle + Math.PI * 0.25)));
        var c3 = Qt.vector3d(Math.abs(Math.sin(angle + Math.PI * 0.25)),
                             Math.abs(Math.sin(angle)),
                             Math.abs(Math.cos(angle)));

        var cols = [c1, c2, c3];

        var byteOffset = 6.0 * 4.0 // 6 * sizeof(float)
        var stride = 9.0 * 4.0 // 9 * sizeof(float)
        var colorArray = new Float32Array(3)
        cols.forEach(function(vec3Col) {
            colorArray[0] = vec3Col.x
            colorArray[1] = vec3Col.y
            colorArray[2] = vec3Col.z
            vertexBuffer.updateData(byteOffset, colorArray)
            byteOffset += stride
        });

        colorBufferTimerUpdate.angle += Math.PI / 360.0;
    }

    QQ2.Timer {
        id: colorBufferTimerUpdate
        interval: 16
        repeat: true
        running: true
        property real angle: 0
        onTriggered: {
            updateVertexColor()
        }
    }

    GeometryRenderer {
        id: customMesh
        instanceCount: 1
        indexOffset: 0
        firstInstance: 0
        primitiveType: GeometryRenderer.Triangles
        Buffer {
            id: vertexBuffer
            type: Buffer.VertexBuffer
            data: buildVertexBufferData()
        }

        Buffer {
            id: indexBuffer
            type: Buffer.IndexBuffer
            data: buildIndexBufferData()
        }
        geometry:  Geometry {
            Attribute {
                attributeType: Attribute.VertexAttribute
                vertexBaseType: Attribute.Float
                vertexSize: 3
                byteOffset: 0
                byteStride: 9 * 4
                count: 4
                name: defaultPositionAttributeName()
                buffer: vertexBuffer
            }

            Attribute {
                attributeType: Attribute.VertexAttribute
                vertexBaseType: Attribute.Float
                vertexSize: 3
                byteOffset: 3 * 4
                byteStride: 9 * 4
                count: 4
                name: defaultNormalAttributeName()
                buffer: vertexBuffer
            }

            Attribute {
                attributeType: Attribute.VertexAttribute
                vertexBaseType: Attribute.Float
                vertexSize: 3
                byteOffset: 6 * 4
                byteStride: 9 * 4
                count: 4
                name: defaultColorAttributeName()
                buffer: vertexBuffer
            }

            Attribute {
                attributeType: Attribute.IndexAttribute
                vertexBaseType: Attribute.UnsignedShort
                vertexSize: 1
                byteOffset: 0
                byteStride: 0
                count: 12
                buffer: indexBuffer
            }
        }
    }

    Transform {
        id: meshTransform
        property real userAngle: 0.0
        scale: 10
        rotation: fromAxisAndAngle(Qt.vector3d(0, 1, 0), userAngle)
    }

    QQ2.NumberAnimation {
        target: meshTransform
        property: "userAngle"
        duration: 10000
        from: 0
        to: 360

        loops: QQ2.Animation.Infinite
        running: true
    }

    Entity {
        id: sphereEntity
        components: [ customMesh, material, meshTransform ]
    }
}
