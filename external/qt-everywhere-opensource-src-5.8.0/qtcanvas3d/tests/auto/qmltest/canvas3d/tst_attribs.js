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
var positionLocation;
var inactiveAttribLocation;
var testAttribLocation;
var shaderProgram;
var buffer;
var vertexShader;
var fragmentShader;

function initializeGL(canvas) {
    var initStatus = 0
    try {
        gl = canvas.getContext("3d");
        buffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
        gl.bufferData(
                    gl.ARRAY_BUFFER, new Float32Array(
                        [-1.0, -1.0,
                         1.0, -1.0,
                         -1.0,  1.0
                        ]),
                    gl.STATIC_DRAW);

        if (!initShaders()) {
            initStatus = 1;
        } else {
            gl.useProgram(shaderProgram);

            positionLocation = gl.getAttribLocation(shaderProgram, "a_position");
            gl.enableVertexAttribArray(positionLocation);
            gl.vertexAttribPointer(positionLocation, 2, gl.FLOAT, false, 0, 0);

            inactiveAttribLocation = gl.getAttribLocation(shaderProgram, "inactiveAttrib");
            testAttribLocation = gl.getAttribLocation(shaderProgram, "testAttrib");

            if (inactiveAttribLocation !== -1
                    || testAttribLocation === -1
                    || positionLocation === -1) {
                initStatus = 2;
            }

            gl.clearColor(0.0, 0.0, 0.0, 1.0);

            gl.viewport(0, 0,
                        canvas.width * canvas.devicePixelRatio,
                        canvas.height * canvas.devicePixelRatio);

            var activeInfo1 = gl.getActiveAttrib(shaderProgram, 0);
            var activeInfo2 = gl.getActiveAttrib(shaderProgram, 1);

            // Note: The indexes given to getActiveAttrib do not necessarily match indexes returned
            // by getAttribLocation, and depend on the driver implementation, so we need to be
            // flexible with our checking.
            if (activeInfo1.size !== 1 || activeInfo2.size !== 1)
                return 3;

            if (activeInfo1.type === Context3D.FLOAT_VEC3) {
                if (activeInfo1.name !== "testAttrib")
                    return 4;
                if (activeInfo2.name !== "a_position")
                    return 5;
                if (activeInfo2.type !== Context3D.FLOAT_VEC2)
                    return 6;
            } else {
                if (activeInfo1.name !== "a_position")
                    return 7;
                if (activeInfo1.type !== Context3D.FLOAT_VEC2)
                    return 8;
                if (activeInfo2.name !== "testAttrib")
                    return 9;
                if (activeInfo2.type !== Context3D.FLOAT_VEC3)
                    return 10;
            }

            // Test generic attribute setting. Attribute zero is special in that it cannot have
            // generic setting, so just skip the test if zero index got assigned to testAttrib.
            if (testAttribLocation !== 0) {
                var colorArray = [0, 1, 1];
                gl.vertexAttrib3fv(testAttribLocation, colorArray);

                var testArray = gl.getVertexAttrib(testAttribLocation, gl.CURRENT_VERTEX_ATTRIB);
                if (testArray[0] !== 0 || testArray[1] !== 1 || testArray[2] !== 1)
                    return 11;
            }
        }
    } catch(e) {
        console.log("initializeGL(): FAILURE!");
        console.log(""+e);
        console.log(""+e.message);
        initStatus = 99;
    }
    return initStatus;
}

function paintGL(canvas) {
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.drawArrays(gl.TRIANGLES, 0, 3);

    return 0;
}

function initShaders()
{
    vertexShader = compileShader("attribute vec2 a_position; \
                                  attribute float inactiveAttrib; \
                                  attribute vec3 testAttrib; \
                                  varying lowp vec3 color; \
                                  void main() { \
                                      gl_Position = vec4(a_position, 1.0, 1.0); \
                                      color = testAttrib;
                                 }", gl.VERTEX_SHADER);
    fragmentShader = compileShader("varying lowp vec3 color; \
                                    void main() { \
                                        gl_FragColor = vec4(color, 1.0); \
                                   }", gl.FRAGMENT_SHADER);

    shaderProgram = gl.createProgram();
    gl.attachShader(shaderProgram, vertexShader);
    gl.attachShader(shaderProgram, fragmentShader);
    gl.linkProgram(shaderProgram);

    // Check linking status
    if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
        console.log("Could not initialize shaders");
        console.log(gl.getProgramInfoLog(shaderProgram));
        return false;
    }

    return true;
}

function compileShader(str, type) {
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
