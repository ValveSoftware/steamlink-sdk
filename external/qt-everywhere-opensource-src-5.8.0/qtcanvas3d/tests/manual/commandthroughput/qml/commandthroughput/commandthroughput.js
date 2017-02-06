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

Qt.include("../../../examples/canvas3d/3rdparty/gl-matrix.js")

//
// Draws a cube that has the Qt logo as decal texture on each face in to a texture.
// That texture is used as the texture for drawing another cube on the screen.
//

var gl;

var cubeTexture = 0;

var vertexPositionAttribute;
var vertexNormalAttribute;
var vertexColorAttribute;
var mvMatrix = mat4.create();
var pMatrix  = mat4.create();
var nMatrix  = mat4.create();
var pMatrixUniform;
var mvMatrixUniform;
var nUniform;

var canvas3d;
var isLogEnabled = false;

function log(message) {
    if (isLogEnabled)
        console.log(message)
}

function initializeGL(canvas, textureLoader) {
    canvas3d = canvas
    try {
        // Get the OpenGL context object that represents the API we call
        gl = canvas.getContext("canvas3d", {depth:true, antialias:true});

        // Setup the OpenGL state
        gl.enable(gl.DEPTH_TEST);
        gl.enable(gl.CULL_FACE);
        gl.cullFace(gl.BACK);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);

        // Initialize the shader program
        initShaders();

        // Initialize vertex and color buffers
        initBuffers();

    } catch(e) {
        console.log("initializeGL FAILURE!");
        console.log(""+e);
        console.log(""+e.message);
    }
}

function degToRad(degrees) {
    return degrees * Math.PI / 180;
}

function paintGL(canvas) {
    gl.viewport(0, 0,
                canvas.width * canvas.devicePixelRatio,
                canvas.height * canvas.devicePixelRatio);

    gl.clearColor(0.0, 0.0, 0.0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    var xStart = -6.0;
    var yStart = -3.2;
    var zStart = -15.0;
    var xRange = 12.0;
    var yRange = 7.0;
    var zRange = 5.0;

    for (var count = 0; count < canvas3d.itemCount; count++) {
        // Calculate and set matrix uniforms
        mat4.perspective(pMatrix, degToRad(45), canvas.width / canvas.height, 0.1, 100.0);
        gl.uniformMatrix4fv(pMatrixUniform, false, pMatrix);

        mat4.identity(mvMatrix);
        var xFrac = ((count % 200) / 200);
        var yFrac = count / canvas3d.maxCount;
        var zFrac = ((count % 12) / 12);
        mat4.translate(mvMatrix, mvMatrix, [xStart + (xFrac * xRange),
                                            yStart + (yFrac * yRange),
                                            zStart + (zFrac * zRange)]);
        mat4.rotate(mvMatrix, mvMatrix, degToRad(canvas.xRotSlider), [0, 1, 0]);
        mat4.rotate(mvMatrix, mvMatrix, degToRad(canvas.yRotSlider), [1, 0, 0]);
        mat4.rotate(mvMatrix, mvMatrix, degToRad(canvas.zRotSlider), [0, 0, 1]);
        mat4.scale(mvMatrix, mvMatrix, [0.15, 0.15, 0.15])
        gl.uniformMatrix4fv(mvMatrixUniform, false, mvMatrix);

        mat4.invert(nMatrix, mvMatrix);
        mat4.transpose(nMatrix, nMatrix);
        gl.uniformMatrix4fv(nUniform, false, nMatrix);

        // Draw the on-screen cube
        gl.drawElements(gl.TRIANGLES, 36, gl.UNSIGNED_SHORT, 0);
    }
}

function onCanvasResize(canvas)
{
    var pixelRatio = canvas.devicePixelRatio;
    canvas.pixelSize = Qt.size(canvas.width * pixelRatio,
                               canvas.height * pixelRatio);
}

function initBuffers()
{
    log("        cubeVertexPositionBuffer");
    var cubeVertexPositionBuffer = gl.createBuffer();
    cubeVertexPositionBuffer.name = "cubeVertexPositionBuffer";
    gl.bindBuffer(gl.ARRAY_BUFFER, cubeVertexPositionBuffer);
    gl.bufferData(
                gl.ARRAY_BUFFER,
                new Float32Array([// Front face
                                        -1.0, -1.0,  1.0,
                                        1.0, -1.0,  1.0,
                                        1.0,  1.0,  1.0,
                                        -1.0,  1.0,  1.0,

                                        // Back face
                                        -1.0, -1.0, -1.0,
                                        -1.0,  1.0, -1.0,
                                        1.0,  1.0, -1.0,
                                        1.0, -1.0, -1.0,

                                        // Top face
                                        -1.0,  1.0, -1.0,
                                        -1.0,  1.0,  1.0,
                                        1.0,  1.0,  1.0,
                                        1.0,  1.0, -1.0,

                                        // Bottom face
                                        -1.0, -1.0, -1.0,
                                        1.0, -1.0, -1.0,
                                        1.0, -1.0,  1.0,
                                        -1.0, -1.0,  1.0,

                                        // Right face
                                        1.0, -1.0, -1.0,
                                        1.0,  1.0, -1.0,
                                        1.0,  1.0,  1.0,
                                        1.0, -1.0,  1.0,

                                        // Left face
                                        -1.0, -1.0, -1.0,
                                        -1.0, -1.0,  1.0,
                                        -1.0,  1.0,  1.0,
                                        -1.0,  1.0, -1.0
                                       ]),
                gl.STATIC_DRAW);
    gl.enableVertexAttribArray(vertexPositionAttribute);
    gl.vertexAttribPointer(vertexPositionAttribute, 3, gl.FLOAT, false, 0, 0);

    log("        cubeVertexIndexBuffer");
    var cubeVertexIndexBuffer = gl.createBuffer();
    cubeVertexIndexBuffer.name = "cubeVertexIndexBuffer";
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, cubeVertexIndexBuffer);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,
                  new Uint16Array([
                                            0,  1,  2,      0,  2,  3,    // front
                                            4,  5,  6,      4,  6,  7,    // back
                                            8,  9,  10,     8,  10, 11,   // top
                                            12, 13, 14,     12, 14, 15,   // bottom
                                            16, 17, 18,     16, 18, 19,   // right
                                            20, 21, 22,     20, 22, 23    // left
                                        ]),
                  gl.STATIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, cubeVertexIndexBuffer);

    var colors = [
                [0.0,  1.0,  1.0,  1.0],    // Front face: white
                [1.0,  0.0,  0.0,  1.0],    // Back face: red
                [0.0,  1.0,  0.0,  1.0],    // Top face: green
                [0.0,  0.0,  1.0,  1.0],    // Bottom face: blue
                [1.0,  1.0,  0.0,  1.0],    // Right face: yellow
                [1.0,  0.0,  1.0,  1.0]     // Left face: purple
            ];

    var generatedColors = [];
    for (var j = 0; j < 6; j++) {
        var c = colors[j];

        for (var i = 0; i < 4; i++) {
            generatedColors = generatedColors.concat(c);
        }
    }
    log("        cubeVertexColorBuffer");
    var cubeVertexColorBuffer = gl.createBuffer();
    cubeVertexColorBuffer.name = "cubeVertexColorBuffer";
    gl.bindBuffer(gl.ARRAY_BUFFER, cubeVertexColorBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(generatedColors), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(vertexColorAttribute);
    gl.vertexAttribPointer(vertexColorAttribute, 4, gl.FLOAT, false, 0, 0);

    log("        cubeVerticesTextureCoordBuffer");
    var cubeVerticesTextureCoordBuffer = gl.createBuffer();
    cubeVerticesTextureCoordBuffer.name = "cubeVerticesTextureCoordBuffer";
    gl.bindBuffer(gl.ARRAY_BUFFER, cubeVerticesTextureCoordBuffer);
    var textureCoordinates = [
                // Front
                1.0,  0.0,
                0.0,  0.0,
                0.0,  1.0,
                1.0,  1.0,
                // Back
                1.0,  0.0,
                0.0,  0.0,
                0.0,  1.0,
                1.0,  1.0,
                // Top
                1.0,  0.0,
                0.0,  0.0,
                0.0,  1.0,
                1.0,  1.0,
                // Bottom
                1.0,  0.0,
                0.0,  0.0,
                0.0,  1.0,
                1.0,  1.0,
                // Right
                1.0,  0.0,
                0.0,  0.0,
                0.0,  1.0,
                1.0,  1.0,
                // Left
                1.0,  0.0,
                0.0,  0.0,
                0.0,  1.0,
                1.0,  1.0
            ];
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(textureCoordinates),
                  gl.STATIC_DRAW);

    var cubeVerticesNormalBuffer = gl.createBuffer();
    cubeVerticesNormalBuffer.name = "cubeVerticesNormalBuffer";
    gl.bindBuffer(gl.ARRAY_BUFFER, cubeVerticesNormalBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
                                                              // Front
                                                              0.0,  0.0,  1.0,
                                                              0.0,  0.0,  1.0,
                                                              0.0,  0.0,  1.0,
                                                              0.0,  0.0,  1.0,

                                                              // Back
                                                              0.0,  0.0, -1.0,
                                                              0.0,  0.0, -1.0,
                                                              0.0,  0.0, -1.0,
                                                              0.0,  0.0, -1.0,

                                                              // Top
                                                              0.0,  1.0,  0.0,
                                                              0.0,  1.0,  0.0,
                                                              0.0,  1.0,  0.0,
                                                              0.0,  1.0,  0.0,

                                                              // Bottom
                                                              0.0, -1.0,  0.0,
                                                              0.0, -1.0,  0.0,
                                                              0.0, -1.0,  0.0,
                                                              0.0, -1.0,  0.0,

                                                              // Right
                                                              1.0,  0.0,  0.0,
                                                              1.0,  0.0,  0.0,
                                                              1.0,  0.0,  0.0,
                                                              1.0,  0.0,  0.0,

                                                              // Left
                                                              -1.0,  0.0,  0.0,
                                                              -1.0,  0.0,  0.0,
                                                              -1.0,  0.0,  0.0,
                                                              -1.0,  0.0,  0.0
                                                          ]), gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, cubeVerticesNormalBuffer);
    gl.vertexAttribPointer(vertexNormalAttribute, 3, gl.FLOAT, false, 0, 0);
}

function initShaders()
{
    var vertexShader = getShader(gl,
                                 "attribute highp vec3 aVertexNormal;   \
                                  attribute highp vec3 aVertexPosition; \
                                  attribute mediump vec4 aVertexColor;  \
                                  attribute highp vec2 aTextureCoord;   \

                                  uniform highp mat4 uNormalMatrix;     \
                                  uniform mat4 uMVMatrix;               \
                                  uniform mat4 uPMatrix;                \

                                  varying mediump vec4 vColor;          \
                                  varying highp vec3 vLighting;         \

                                  void main(void) {                     \
                                      gl_Position = uPMatrix * uMVMatrix * vec4(aVertexPosition, 1.0);                      \
                                      vColor = aVertexColor;                                                                \
                                      highp vec3 ambientLight = vec3(0.5, 0.5, 0.5);                                        \
                                      highp vec3 directionalLightColor = vec3(0.15, 0.15, 0.15);                             \
                                      highp vec3 directionalVector = vec3(0.85, 0.8, 0.75);                                 \
                                      highp vec4 transformedNormal = uNormalMatrix * vec4(aVertexNormal, 1.0);              \
                                      highp float directional = max(dot(transformedNormal.xyz, directionalVector), 0.0);    \
                                      vLighting = ambientLight + (directionalLightColor * directional);                     \
                                  }", gl.VERTEX_SHADER);
    var fragmentShader = getShader(gl,
                                   "varying mediump vec4 vColor;        \
                                    varying highp vec3 vLighting;       \

                                    void main(void) {                   \
                                        gl_FragColor = vec4(vColor.rgb * vLighting, 1.0);                                       \
                                    }", gl.FRAGMENT_SHADER);

    var shaderProgram = gl.createProgram();
    gl.attachShader(shaderProgram, vertexShader);
    gl.attachShader(shaderProgram, fragmentShader);
    gl.linkProgram(shaderProgram);

    if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
        console.log("Could not initialize shaders");
        console.log(gl.getProgramInfoLog(shaderProgram));
    }

    gl.useProgram(shaderProgram);

    // look up where the vertex data needs to go.
    vertexPositionAttribute = gl.getAttribLocation(shaderProgram, "aVertexPosition");
    gl.enableVertexAttribArray(vertexPositionAttribute);
    vertexColorAttribute = gl.getAttribLocation(shaderProgram, "aVertexColor");
    gl.enableVertexAttribArray(vertexColorAttribute);
    vertexNormalAttribute =gl.getAttribLocation(shaderProgram, "aVertexNormal");
    gl.enableVertexAttribArray(vertexNormalAttribute);

    pMatrixUniform  = gl.getUniformLocation(shaderProgram, "uPMatrix");
    mvMatrixUniform = gl.getUniformLocation(shaderProgram, "uMVMatrix");
    nUniform = gl.getUniformLocation(shaderProgram, "uNormalMatrix");
}

function getShader(gl, str, type) {
    var shader = gl.createShader(type);
    gl.shaderSource(shader, str);
    gl.compileShader(shader);

    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        console.log("JS:Shader compile failed");
        console.log(gl.getShaderInfoLog(shader));
        return null;
    }

    return shader;
}
