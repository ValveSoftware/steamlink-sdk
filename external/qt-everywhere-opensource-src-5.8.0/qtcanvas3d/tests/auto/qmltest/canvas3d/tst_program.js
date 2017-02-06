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
var inactiveUniformLocation;
var testUniformLocation;
var shaderProgram;
var buffer;
var vertexShader;
var fragmentShader;

function compareValues(a, b) {
    if (a !== b) {
        console.log("initializeGL(): FAILURE: value doesn't have expected contents.",
                    "Expected:", a, "Actual:", b);
        return false;
    }

    return true;
}

function getShaderPrecisionFormat(a, b, errorExpected) {
    var precFormat = gl.getShaderPrecisionFormat(a, b);

    // The returned values may vary depending on the platform, so just check that we don't get
    // the uninitialized values or error.

    var glError = gl.getError();
    if (glError !== 0) {
        if (errorExpected)
            return true;

        console.log("GL error:", glError, "when calling getShaderPrecisionFormat()");
        return false;
    }

    if (precFormat.rangeMin === 1 || precFormat.rangeMax === 1 || precFormat.precision === 1) {
        console.log("Default values returned when calling getShaderPrecisionFormat");
        return false;
    }

    return true;
}

function isProgram(a, expectedValue) {
    var varIsProgram = gl.isProgram(a);
    var glError = gl.getError();
    if (glError !== 0) {
        console.log("GL error:", glError, "when calling isProgram()");
        return false;
    }

    if (expectedValue === varIsProgram)
        return true;
    else
        return false;
}

function isShader(a, expectedValue) {
    var varIsShader = gl.isShader(a);
    var glError = gl.getError();
    if (glError !== 0) {
        console.log("GL error:", glError, "when calling isShader()");
        return false;
    }

    if (expectedValue === varIsShader)
        return true;
    else
        return false;
}

function deleteProgram(a, errorExpected) {
    gl.deleteProgram(a);

    var glError = gl.getError();
    if (glError !== 0) {
        if (errorExpected)
            return true;

        console.log("GL error:", glError, "when calling deleteProgram()");
        return false;
    }

    return true;
}

function deleteShader(a, errorExpected) {
    gl.deleteShader(a);

    var glError = gl.getError();
    if (glError !== 0) {
        if (errorExpected)
            return true;

        console.log("GL error:", glError, "when calling deleteShader()");
        return false;
    }

    return true;
}

function getAttachedShaders(a, count, errorExpected) {
    var shaderList = gl.getAttachedShaders(a);

    var glError = gl.getError();
    if (glError !== 0) {
        if (errorExpected)
            return true;
        console.log("GL error:", glError, "when calling getAttachedShaders()");
        return false;
    }

    if (shaderList.length !== count) {
        console.log("Wrong shader count, expected: ", count, ", Actual: ", shaderList.length);
        return false;
    }

    return true;
}

function detachShader(a, b, errorExpected) {
    gl.detachShader(a, b);

    var glError = gl.getError();
    if (glError !== 0) {
        if (errorExpected)
            return true;
        console.log("GL error:", glError, "when calling detachShader()");
        return false;
    }

    return true;
}

function getShaderSource(a, checkStr, notFoundExpected, errorExpected) {
    var srcString = gl.getShaderSource(a);

    var glError = gl.getError();
    if (glError !== 0) {
        if (errorExpected)
            return true;
        console.log("GL error:", glError, "when calling getShaderSource()");
        return false;
    }

    if (srcString.indexOf(checkStr) === -1) {
        if (notFoundExpected)
            return true;
        console.log("Invalid shader string when calling getShaderSource()");
        return false;
    }

    return true;
}

function getShaderInfoLog(a, errorExpected) {
    var infoStr = gl.getShaderInfoLog(a);

    var glError = gl.getError();
    if (glError !== 0) {
        if (errorExpected)
            return true;
        console.log("GL error:", glError, "when calling getShaderInfoLog()");
        return false;
    }

    if (infoStr === "") {
        console.log("Empty log when calling getShaderInfoLog()");
        return false;
    }

    return true;
}

function getProgramInfoLog(a, errorExpected) {
    var infoStr = gl.getProgramInfoLog(a);

    var glError = gl.getError();
    if (glError !== 0) {
        if (errorExpected)
            return true;
        console.log("GL error:", glError, "when calling getProgramInfoLog()");
        return false;
    }

    if (infoStr === "") {
        console.log("Empty log when calling getProgramInfoLog()");
        return false;
    }

    return true;
}

function initializeGL(canvas) {
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

        if (!initShaders())
            return 1;

        if (!getAttachedShaders(shaderProgram, 2, false))
            return 2;

        if (!deleteProgram(shaderProgram, false))
            return 3;
        // Deleting same again should be silently ignored
        if (!deleteProgram(shaderProgram, false))
            return 4;
        if (!deleteProgram(vertexShader, true))
            return 5;

        if (!getAttachedShaders(shaderProgram, 0, true))
            return 6;

        // Reinitialize shaders
        if (!initShaders())
            return 7;

        if (!getAttachedShaders(shaderProgram, 2, false))
            return 8;

        if (!detachShader(shaderProgram, vertexShader, false))
            return 9;

        if (!getAttachedShaders(shaderProgram, 1, false))
            return 10;

        // Detach again should generate error
        if (!detachShader(shaderProgram, vertexShader, true))
            return 11;

        if (!getAttachedShaders(shaderProgram, 1, false))
            return 12;

        // Reinitialize shaders
        if (!initShaders())
            return 13;

        if (!deleteShader(shaderProgram, true))
            return 14;
        if (!deleteShader(vertexShader, false))
            return 15;
        // Deleting same again should be silently ignored
        if (!deleteShader(vertexShader, false))
            return 16;

        // Reinitialize shaders
        if (!initShaders())
            return 17;

        gl.useProgram(shaderProgram);

        positionLocation = gl.getAttribLocation(shaderProgram, "a_position");
        gl.enableVertexAttribArray(positionLocation);
        gl.vertexAttribPointer(positionLocation, 2, gl.FLOAT, false, 0, 0);

        gl.clearColor(0.0, 0.0, 0.0, 1.0);

        gl.viewport(0, 0,
                    canvas.width * canvas.devicePixelRatio,
                    canvas.height * canvas.devicePixelRatio);

        if (!getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.LOW_FLOAT, false))
            return 20;
        if (!getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.MEDIUM_FLOAT, false))
            return 21;
        if (!getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.HIGH_FLOAT, false))
            return 22;
        if (!getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.LOW_INT, false))
            return 23;
        if (!getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.MEDIUM_INT, false))
            return 24;
        if (!getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.HIGH_INT, false))
            return 25;
        if (!getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.LOW_FLOAT, false))
            return 26;
        if (!getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.MEDIUM_FLOAT, false))
            return 27;
        if (!getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.HIGH_FLOAT, false))
            return 28;
        if (!getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.LOW_INT, false))
            return 29;
        if (!getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.MEDIUM_INT, false))
            return 30;
        if (!getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.HIGH_INT, false))
            return 31;
        if (!getShaderPrecisionFormat(77, gl.LOW_INT, true))
            return 32;
        if (!getShaderPrecisionFormat(gl.FRAGMENT_SHADER, 77, true))
            return 33;

        if (!isProgram(shaderProgram, true))
            return 40;
        if (!isProgram(vertexShader, false))
            return 41;
        if (!isShader(shaderProgram, false))
            return 42;
        if (!isShader(vertexShader, true))
            return 43;

        if (!getShaderSource(vertexShader, "testVertexString", false, false))
            return 60;
        if (!getShaderSource(fragmentShader, "testFragmentString", false, false))
            return 61;
        if (!getShaderSource(vertexShader, "testFragmentString", true, false))
            return 62;
        if (!getShaderSource(fragmentShader, "testVertexString", true, false))
            return 63;
        if (!getShaderSource(shaderProgram, "foobar", false, true))
            return 64;

        // Tests checking info log contents are not reliable, as it is entirely up to the
        // OpenGL implementation what to return in the info log, if anything.
        // Since it's unreliable, let's skip these tests.
//        var dummyShader = gl.createShader(gl.FRAGMENT_SHADER);
//        gl.shaderSource(dummyShader, "This shader is dummy");
//        gl.compileShader(dummyShader);
//        gl.getError(); // Clear the error from faulty compilation
//        var dummyProgram = gl.createProgram();
//        gl.attachShader(dummyProgram, dummyShader);
//        gl.linkProgram(dummyProgram);
//        gl.getError(); // Clear the error from faulty linking

//        if (!getShaderInfoLog(dummyShader, false))
//            return 70;
//        if (!getShaderInfoLog(dummyProgram, true))
//            return 71;

//        if (!getProgramInfoLog(dummyProgram, false))
//            return 72;
//        if (!getProgramInfoLog(dummyShader, true))
//            return 73;
    } catch(e) {
        console.log("initializeGL(): FAILURE!");
        console.log(""+e);
        console.log(""+e.message);
        return -1;
    }
    return 0;
}

function paintGL(canvas) {
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.drawArrays(gl.TRIANGLES, 0, 3);

    return 0;
}

function initShaders()
{
    vertexShader = compileShader("attribute highp vec2 a_position; \
                                  varying highp vec4 varPosition; \
                                  attribute highp float testVertexString; \
                                  void main() { \
                                      gl_Position = vec4(a_position, 0.0, 1.0); \
                                      varPosition = gl_Position + vec4(1.0, 1.0, 1.0, 1.0); \
                                      varPosition /= 2.0; \
                                  }", gl.VERTEX_SHADER);
    fragmentShader = compileShader("varying highp vec4 varPosition; \
                                    uniform highp float testFragmentString; \
                                    void main() { \
                                        gl_FragColor = varPosition; \
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

    if (gl.getShaderParameter(shader, gl.SHADER_TYPE) !== type) {
        console.log("JS:Shader type test failed");
        console.log(gl.getShaderInfoLog(shader));
        return null;
    }

    return shader;
}
