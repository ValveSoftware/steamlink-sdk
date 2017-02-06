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

var gl;

var colorBuffer;

var vertexPositionAttribute;
var vertexColorAttribute;

var vertexBuffer;

function initializeGL(canvas) {
    gl = canvas.getContext("", {alpha:true, premultipliedAlpha:false, antialias:true});
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
    var pixelRatio = canvas.devicePixelRatio;
    gl.viewport(0, 0, pixelRatio * canvas.width, pixelRatio * canvas.height);

    initShaders();
    initBuffers();
}

function paintGL() {
    // Clear color has zero alpha, so after premultiplication it should have no effect on
    // components underlying the canvas when not using premultiplied color values.
    gl.clearColor(1.0, 1.0, 1.0, 0.0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
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
    vertexBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
    gl.bufferData(
                gl.ARRAY_BUFFER, new Float32Array(
                    [-1.0, -1.0,
                     1.0, -1.0,
                     -1.0,  1.0
                    ]),
                gl.STATIC_DRAW);

    gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
    gl.vertexAttribPointer(vertexPositionAttribute, 2, gl.FLOAT, false, 0, 0);
    gl.enableVertexAttribArray(vertexPositionAttribute);

    colorBuffer = gl.createBuffer();
    setColor(0.0, 0.0, 255.0, 255.0);
}

function initShaders()
{
    var vertexShader = getShader(
                gl,
                "attribute highp vec2 aVertexPosition;                 \
                attribute mediump vec4 aVertexColor;                   \
                varying mediump vec4 vColor;                           \
                void main(void) {                                      \
                    gl_Position = vec4(aVertexPosition, 0.0, 1.0); \
                    vColor = aVertexColor;                             \
                }",
                gl.VERTEX_SHADER);
    var fragmentShader = getShader(
                gl,
                "varying mediump vec4 vColor;                   \
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
