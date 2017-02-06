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

Qt.include("../../../../examples/canvas3d/3rdparty/gl-matrix.js")

var gl;

var mvMatrix = mat4.create();
var pMatrix = mat4.create();

var colorBuffer;

var pMatrixUniform;
var mvMatrixUniform;

var vertexPositionAttribute;
var vertexColorAttribute;

function initializeGL(canvas) {
    gl = canvas.getContext("");
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
    var pixelRatio = canvas.devicePixelRatio;
    gl.viewport(0, 0, pixelRatio * canvas.width, pixelRatio * canvas.height);

    initShaders();
    initBuffers();

    mat4.perspective(pMatrix, degToRad(45), canvas.width / canvas.height, 0.1, 500.0);
    gl.uniformMatrix4fv(pMatrixUniform, false, pMatrix);
    mat4.identity(mvMatrix);
    mat4.translate(mvMatrix, mvMatrix, [0, 0, -5]);
    gl.uniformMatrix4fv(mvMatrixUniform, false, mvMatrix);
}

function paintGL() {
    gl.clearColor(1.0, 0.0, 0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.drawElements(gl.TRIANGLES, 6, gl.UNSIGNED_SHORT, 0);
}

function paintGL(x, y) {
    gl.clearColor(64.0 / 255.0, 80.0 / 255.0, 96.0 / 255.0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    gl.drawElements(gl.TRIANGLES, 6, gl.UNSIGNED_SHORT, 0);

    var pixels = checkPixel(x, y);

    return pixels;
}

function checkPixel(x, y) {
    var pixels = new Uint8Array(4);
    gl.readPixels(x, y, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    return pixels;
}

function setColor(r, g, b, a) {
    gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
    var colors = [r / 255.0, g / 255.0, b / 255.0, a / 255.0];
    var generatedColors = [];
    for (var i = 0; i < 4; i++)
        generatedColors = generatedColors.concat(colors);
    gl.bufferData(gl.ARRAY_BUFFER,
                  new Float32Array(generatedColors),
                  gl.STATIC_DRAW);
    gl.enableVertexAttribArray(vertexColorAttribute);
    gl.vertexAttribPointer(vertexColorAttribute, 4, gl.FLOAT, false, 0, 0);
}

function initBuffers()
{
    var vertexBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
    gl.bufferData(gl.ARRAY_BUFFER,
                  new Float32Array([-1.0, -1.0, 1.0,
                                    1.0, -1.0, 1.0,
                                    1.0, 1.0, 1.0,
                                    -1.0, 1.0, 1.0]),
                  gl.STATIC_DRAW);
    gl.enableVertexAttribArray(vertexPositionAttribute);
    gl.vertexAttribPointer(vertexPositionAttribute, 3, gl.FLOAT, false, 0, 0);

    colorBuffer = gl.createBuffer();
    setColor(16.0, 32.0, 48.0, 255.0);

    var uvBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, uvBuffer);
    gl.bufferData(gl.ARRAY_BUFFER,
                  new Float32Array([1.0, 0.0,
                                    0.0, 0.0,
                                    0.0, 1.0,
                                    1.0, 1.0]),
                  gl.STATIC_DRAW);

    var indexBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, indexBuffer);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,
                  new Uint16Array([0, 1, 2,
                                   0, 2, 3]),
                  gl.STATIC_DRAW);
}

function initShaders()
{
    var vertexShader = getShader(
                gl,
                "attribute highp vec3 aVertexPosition;                 \
                attribute mediump vec4 aVertexColor;                   \
                uniform mat4 uMVMatrix;                                \
                uniform mat4 uPMatrix;                                 \
                varying mediump vec4 vColor;                           \
                void main(void) {                                      \
                    gl_Position = uPMatrix * uMVMatrix * vec4(aVertexPosition, 1.0); \
                    vColor = aVertexColor;                             \
                }",
                gl.VERTEX_SHADER);
    var fragmentShader = getShader(
                gl,
                "varying mediump vec4 vColor;                   \
                uniform sampler2D uSampler;                     \
                void main(void) {                               \
                    gl_FragColor = vColor;                      \
                }",
                gl.FRAGMENT_SHADER);

    var shaderProgram = gl.createProgram();
    gl.attachShader(shaderProgram, vertexShader);
    gl.attachShader(shaderProgram, fragmentShader);
    gl.linkProgram(shaderProgram);
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);

    if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
        console.log("Could not initialize shaders");
        console.log(gl.getProgramInfoLog(shaderProgram));
    }

    gl.useProgram(shaderProgram);

    vertexPositionAttribute = gl.getAttribLocation(shaderProgram, "aVertexPosition");
    gl.enableVertexAttribArray(vertexPositionAttribute);
    vertexColorAttribute = gl.getAttribLocation(shaderProgram, "aVertexColor");
    gl.enableVertexAttribArray(vertexColorAttribute);

    pMatrixUniform = gl.getUniformLocation(shaderProgram, "uPMatrix");
    mvMatrixUniform = gl.getUniformLocation(shaderProgram, "uMVMatrix");
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

function degToRad(degrees) {
    return degrees * Math.PI / 180;
}
