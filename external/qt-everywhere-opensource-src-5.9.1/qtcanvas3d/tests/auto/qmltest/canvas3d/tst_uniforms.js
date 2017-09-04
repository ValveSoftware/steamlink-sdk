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

var uniform1i;
var uniform2i;
var uniform3i;
var uniform4i;
var uniform1f;
var uniform2f;
var uniform3f;
var uniform4f;
var uniform1b;
var uniform2b;
var uniform3b;
var uniform4b;
var uniform1iv;
var uniform2iv;
var uniform3iv;
var uniform4iv;
var uniform1fv;
var uniform2fv;
var uniform3fv;
var uniform4fv;
var uniform1bv;
var uniform2bv;
var uniform3bv;
var uniform4bv;
var uniformMatrix2fv;
var uniformMatrix3fv;
var uniformMatrix4fv;
var uniformSampler;

var fillValue;
var testArray;

function constructTestArrayJS(count) {
    testArray = [];
    for (var i = 0; i < count; i++) {
        testArray[i] = fillValue++;
    }
    return testArray;
}

function constructTestArrayBooleanJS(count) {
    testArray = [];
    for (var i = 0; i < count; i++) {
        testArray[i] = Boolean(fillValue++ % 2);
    }
    return testArray;
}

function constructTestArrayInt(count) {
    testArray = new Int32Array(count);
    for (var i = 0; i < count; i++) {
        testArray[i] = fillValue++;
    }
    return testArray;
}

function constructTestArrayFloat(count) {
    testArray = new Float32Array(count);
    for (var i = 0; i < count; i++) {
        testArray[i] = fillValue++;
    }
    return testArray;
}

function compareUniformValues(a, b) {
    if (a !== b) {
        console.log("initializeGL(): FAILURE: returned uniform doesn't have expected contents.",
                    "Expected:", a, "Actual:", b);
        return false;
    }

    return true;
}

function compareUniformArrays(a, b) {
    if (a.length !== b.length) {
        console.log("initializeGL(): FAILURE: returned uniform array not the expected length. Expected:",
                    a.length, "Actual:", b.length);
        return false;
    }

    for (var i = 0; i < a.length; i++) {
        if (a[i] !== b[i]) {
            console.log("initializeGL(): FAILURE: returned uniform array doesn't have expected contents.",
                        "Expected:", a[i], "Actual:", b[i], "Index:", i);
            return false;
        }
    }

    return true;
}

function compareUniformSubArrays(index, len, a, b) {
    var newArray = [];

    if (len !== b.length) {
        console.log("initializeGL(): FAILURE: returned uniform arrays not the expected length. Expected:",
                    a.length - index, "Actual:", b.length);
        return false;
    }

    for (var i = 0; i < len; i++) {
        newArray[i] = a[i + index];
    }

    return compareUniformArrays(newArray, b);
}
function compareUniformSubArraysTransposed(index, len, a, b) {
    var newArray = [];

    if (len !== b.length) {
        console.log("initializeGL(): FAILURE: returned uniform arrays not the expected length. Expected:",
                    a.length - index, "Actual:", b.length);
        return false;
    }

    var dim = Math.sqrt(len);

    // Transpose the original array
    var newIndex = 0;
    for (var i = 0; i < dim; i++) {
        for (var j = 0; j < dim; j++) {
            newArray[newIndex++] = a[index + (j * dim) + i];
        }
    }

    return compareUniformArrays(newArray, b);
}

function debugPrintArray(title, a) {
    var len = a.length;
    console.log(title);
    for (var i = 0; i < len; i++) {
        console.log(i, ":", a[i]);
    }
}

function getUniformValue(uniformName) {
    var uniformLocation = gl.getUniformLocation(shaderProgram, uniformName);
    var uniformValue = gl.getUniform(shaderProgram, uniformLocation);

    return uniformValue;
}

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

            inactiveUniformLocation = gl.getUniformLocation(shaderProgram, "inactiveUniform");
            testUniformLocation = gl.getUniformLocation(shaderProgram, "testUniform");

            uniform1i = gl.getUniformLocation(shaderProgram, "i1");
            uniform2i = gl.getUniformLocation(shaderProgram, "i2");
            uniform3i = gl.getUniformLocation(shaderProgram, "i3");
            uniform4i = gl.getUniformLocation(shaderProgram, "i4");
            uniform1f = gl.getUniformLocation(shaderProgram, "f1");
            uniform2f = gl.getUniformLocation(shaderProgram, "f2");
            uniform3f = gl.getUniformLocation(shaderProgram, "f3");
            uniform4f = gl.getUniformLocation(shaderProgram, "f4");
            uniform1b = gl.getUniformLocation(shaderProgram, "b1");
            uniform2b = gl.getUniformLocation(shaderProgram, "b2");
            uniform3b = gl.getUniformLocation(shaderProgram, "b3");
            uniform4b = gl.getUniformLocation(shaderProgram, "b4");
            uniform1iv = gl.getUniformLocation(shaderProgram, "i1v");
            uniform2iv = gl.getUniformLocation(shaderProgram, "i2v");
            uniform3iv = gl.getUniformLocation(shaderProgram, "i3v");
            uniform4iv = gl.getUniformLocation(shaderProgram, "i4v");
            uniform1fv = gl.getUniformLocation(shaderProgram, "f1v");
            uniform2fv = gl.getUniformLocation(shaderProgram, "f2v");
            uniform3fv = gl.getUniformLocation(shaderProgram, "f3v");
            uniform4fv = gl.getUniformLocation(shaderProgram, "f4v");
            uniform1bv = gl.getUniformLocation(shaderProgram, "b1v");
            uniform2bv = gl.getUniformLocation(shaderProgram, "b2v");
            uniform3bv = gl.getUniformLocation(shaderProgram, "b3v");
            uniform4bv = gl.getUniformLocation(shaderProgram, "b4v");
            uniformMatrix2fv = gl.getUniformLocation(shaderProgram, "matrix2");
            uniformMatrix3fv = gl.getUniformLocation(shaderProgram, "matrix3");
            uniformMatrix4fv = gl.getUniformLocation(shaderProgram, "matrix4");
            uniformSampler = gl.getUniformLocation(shaderProgram, "uSampler")

            gl.activeTexture(gl.TEXTURE0);
            gl.uniform1i(uniformSampler, 0);

            gl.clearColor(0.0, 0.0, 0.0, 1.0);

            gl.viewport(0, 0,
                        canvas.width * canvas.devicePixelRatio,
                        canvas.height * canvas.devicePixelRatio);

            // Test getActiveUniform
            var uniformCount = 0;
            var expectedUniformCount = 37;
            var actualUniformCount = gl.getProgramParameter(shaderProgram, gl.ACTIVE_UNIFORMS);

            for (var i = 0; i < actualUniformCount; i++) {
                var activeInfo = gl.getActiveUniform(shaderProgram, i);
                if (activeInfo.name !== "")
                    uniformCount++;
                if (activeInfo.name === "testUniform") {
                    if (activeInfo.size !== 1) {
                        console.log("initializeGL(): FAILURE: testUniform size wrong");
                        return 2;
                    }

                    if (activeInfo.type !== Context3D.FLOAT) {
                        console.log("initializeGL(): FAILURE: testUniform type wrong");
                        return 3;
                    }
                }
            }

            if (uniformCount !== expectedUniformCount) {
                console.log("initializeGL(): FAILURE: active uniform count wrong, expected:",
                            expectedUniformCount, "actual:", uniformCount);
                return 4;
            }

            // Test uniform setters and getters
            var uniformValue = 0;
            var checkValue = false;
            fillValue = 1;

            gl.uniform1i(uniform1i, fillValue);
            uniformValue = gl.getUniform(shaderProgram, uniform1i);
            if (!compareUniformValues(fillValue, uniformValue))
                return 5;
            fillValue++;

            constructTestArrayJS(2);
            gl.uniform2i(uniform2i, testArray[0], testArray[1]);
            uniformValue = gl.getUniform(shaderProgram, uniform2i);
            if (!compareUniformArrays(testArray, uniformValue))
                return 6;

            constructTestArrayJS(3);
            gl.uniform3i(uniform3i, testArray[0], testArray[1], testArray[2]);
            uniformValue = gl.getUniform(shaderProgram, uniform3i);
            if (!compareUniformArrays(testArray, uniformValue))
                return 7;

            constructTestArrayJS(4);
            gl.uniform4i(uniform4i, testArray[0], testArray[1], testArray[2], testArray[3]);
            uniformValue = gl.getUniform(shaderProgram, uniform4i);
            if (!compareUniformArrays(testArray, uniformValue))
                return 8;

            gl.uniform1f(uniform1f, fillValue);
            uniformValue = gl.getUniform(shaderProgram, uniform1f);
            if (!compareUniformValues(fillValue, uniformValue))
                return 9;
            fillValue++;

            constructTestArrayJS(2);
            gl.uniform2f(uniform2f, testArray[0], testArray[1]);
            uniformValue = gl.getUniform(shaderProgram, uniform2f);
            if (!compareUniformArrays(testArray, uniformValue))
                return 10;

            constructTestArrayJS(3);
            gl.uniform3f(uniform3f, testArray[0], testArray[1], testArray[2]);
            uniformValue = gl.getUniform(shaderProgram, uniform3f);
            if (!compareUniformArrays(testArray, uniformValue))
                return 11;

            constructTestArrayJS(4);
            gl.uniform4f(uniform4f, testArray[0], testArray[1], testArray[2], testArray[3]);
            uniformValue = gl.getUniform(shaderProgram, uniform4f);
            if (!compareUniformArrays(testArray, uniformValue))
                return 12;

            gl.uniform1i(uniform1b, true);
            uniformValue = gl.getUniform(shaderProgram, uniform1b);
            if (!compareUniformValues(true, uniformValue))
                return 13;
            fillValue++;

            constructTestArrayBooleanJS(2);
            gl.uniform2i(uniform2b, testArray[0], testArray[1]);
            uniformValue = gl.getUniform(shaderProgram, uniform2b);
            if (!compareUniformArrays(testArray, uniformValue))
                return 14;

            constructTestArrayBooleanJS(3);
            gl.uniform3i(uniform3b, testArray[0], testArray[1], testArray[2]);
            uniformValue = gl.getUniform(shaderProgram, uniform3b);
            if (!compareUniformArrays(testArray, uniformValue))
                return 15;

            constructTestArrayBooleanJS(4);
            gl.uniform4i(uniform4b, testArray[0], testArray[1], testArray[2], testArray[3]);
            uniformValue = gl.getUniform(shaderProgram, uniform4b);
            if (!compareUniformArrays(testArray, uniformValue))
                return 16;

            // Test both javascript arrays and typed arrays for functions that take arrays
            var arrayElements = 3;
            var elementLen = 1;
            gl.uniform1iv(uniform1iv, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("i1v[0]");
            if (!compareUniformValues(testArray[0], uniformValue))
                return 17;
            uniformValue = getUniformValue("i1v[1]");
            if (!compareUniformValues(testArray[1], uniformValue))
                return 18;
            uniformValue = getUniformValue("i1v[2]");
            if (!compareUniformValues(testArray[2], uniformValue))
                return 19;

            elementLen = 2;
            gl.uniform2iv(uniform2iv, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("i2v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 20;
            uniformValue = getUniformValue("i2v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 21;
            uniformValue = getUniformValue("i2v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 22;

            elementLen = 3;
            gl.uniform3iv(uniform3iv, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("i3v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 23;
            uniformValue = getUniformValue("i3v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 24;
            uniformValue = getUniformValue("i3v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 25;

            elementLen = 4;
            gl.uniform4iv(uniform4iv, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("i4v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 26;
            uniformValue = getUniformValue("i4v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 27;
            uniformValue = getUniformValue("i4v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 28;

            elementLen = 1;
            gl.uniform1fv(uniform1fv, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("f1v[0]");
            if (!compareUniformValues(testArray[0], uniformValue))
                return 29;
            uniformValue = getUniformValue("f1v[1]");
            if (!compareUniformValues(testArray[1], uniformValue))
                return 30;
            uniformValue = getUniformValue("f1v[2]");
            if (!compareUniformValues(testArray[2], uniformValue))
                return 31;

            elementLen = 2;
            gl.uniform2fv(uniform2fv, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("f2v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 32;
            uniformValue = getUniformValue("f2v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 33;
            uniformValue = getUniformValue("f2v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 34;

            elementLen = 3;
            gl.uniform3fv(uniform3fv, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("f3v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 35;
            uniformValue = getUniformValue("f3v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 36;
            uniformValue = getUniformValue("f3v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 37;

            elementLen = 4;
            gl.uniform4fv(uniform4fv, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("f4v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 38;
            uniformValue = getUniformValue("f4v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 39;
            uniformValue = getUniformValue("f4v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 40;

            elementLen = 1
            gl.uniform1iv(uniform1bv, constructTestArrayBooleanJS(arrayElements * elementLen));

            uniformValue = getUniformValue("b1v[0]");
            if (!compareUniformValues(testArray[0], uniformValue))
                return 41;
            uniformValue = getUniformValue("b1v[1]");
            if (!compareUniformValues(testArray[1], uniformValue))
                return 42;
            uniformValue = getUniformValue("b1v[2]");
            if (!compareUniformValues(testArray[2], uniformValue))
                return 43;

            elementLen = 2;
            gl.uniform2iv(uniform2bv, constructTestArrayBooleanJS(arrayElements * elementLen));

            uniformValue = getUniformValue("b2v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 44;
            uniformValue = getUniformValue("b2v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 45;
            uniformValue = getUniformValue("b2v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 46;

            elementLen = 3;
            gl.uniform3iv(uniform3bv, constructTestArrayBooleanJS(arrayElements * elementLen));

            uniformValue = getUniformValue("b3v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 47;
            uniformValue = getUniformValue("b3v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 48;
            uniformValue = getUniformValue("b3v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 49;

            elementLen = 4;
            gl.uniform4iv(uniform4bv, constructTestArrayBooleanJS(arrayElements * elementLen));

            uniformValue = getUniformValue("b4v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 50;
            uniformValue = getUniformValue("b4v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 51;
            uniformValue = getUniformValue("b4v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 52;

            elementLen = 2 * 2;
            gl.uniformMatrix2fv(uniformMatrix2fv, false, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix2[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 53;
            uniformValue = getUniformValue("matrix2[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 54;
            uniformValue = getUniformValue("matrix2[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 55;

            gl.uniformMatrix2fv(uniformMatrix2fv, true, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix2[0]");
            if (!compareUniformSubArraysTransposed(0 * elementLen, elementLen, testArray, uniformValue))
                return 56;
            uniformValue = getUniformValue("matrix2[1]");
            if (!compareUniformSubArraysTransposed(1 * elementLen, elementLen, testArray, uniformValue))
                return 57;
            uniformValue = getUniformValue("matrix2[2]");
            if (!compareUniformSubArraysTransposed(2 * elementLen, elementLen, testArray, uniformValue))
                return 58;

            elementLen = 3 * 3;
            gl.uniformMatrix3fv(uniformMatrix3fv, false, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix3[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 59;
            uniformValue = getUniformValue("matrix3[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 60;
            uniformValue = getUniformValue("matrix3[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 61;

            gl.uniformMatrix3fv(uniformMatrix3fv, true, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix3[0]");
            if (!compareUniformSubArraysTransposed(0 * elementLen, elementLen, testArray, uniformValue))
                return 62;
            uniformValue = getUniformValue("matrix3[1]");
            if (!compareUniformSubArraysTransposed(1 * elementLen, elementLen, testArray, uniformValue))
                return 63;
            uniformValue = getUniformValue("matrix3[2]");
            if (!compareUniformSubArraysTransposed(2 * elementLen, elementLen, testArray, uniformValue))
                return 64;

            elementLen = 4 * 4;
            gl.uniformMatrix4fv(uniformMatrix4fv, false, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix4[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 65;
            uniformValue = getUniformValue("matrix4[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 66;
            uniformValue = getUniformValue("matrix4[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 67;

            gl.uniformMatrix4fv(uniformMatrix4fv, true, constructTestArrayJS(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix4[0]");
            if (!compareUniformSubArraysTransposed(0 * elementLen, elementLen, testArray, uniformValue))
                return 68;
            uniformValue = getUniformValue("matrix4[1]");
            if (!compareUniformSubArraysTransposed(1 * elementLen, elementLen, testArray, uniformValue))
                return 69;
            uniformValue = getUniformValue("matrix4[2]");
            if (!compareUniformSubArraysTransposed(2 * elementLen, elementLen, testArray, uniformValue))
                return 70;

            elementLen = 1;
            gl.uniform1iv(uniform1iv, constructTestArrayInt(arrayElements * elementLen));

            uniformValue = getUniformValue("i1v[0]");
            if (!compareUniformValues(testArray[0], uniformValue))
                return 71;
            uniformValue = getUniformValue("i1v[1]");
            if (!compareUniformValues(testArray[1], uniformValue))
                return 72;
            uniformValue = getUniformValue("i1v[2]");
            if (!compareUniformValues(testArray[2], uniformValue))
                return 73;

            elementLen = 2;
            gl.uniform2iv(uniform2iv, constructTestArrayInt(arrayElements * elementLen));

            uniformValue = getUniformValue("i2v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 74;
            uniformValue = getUniformValue("i2v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 75;
            uniformValue = getUniformValue("i2v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 76;

            elementLen = 3;
            gl.uniform3iv(uniform3iv, constructTestArrayInt(arrayElements * elementLen));

            uniformValue = getUniformValue("i3v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 77;
            uniformValue = getUniformValue("i3v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 78;
            uniformValue = getUniformValue("i3v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 79;

            elementLen = 4;
            gl.uniform4iv(uniform4iv, constructTestArrayInt(arrayElements * elementLen));

            uniformValue = getUniformValue("i4v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 80;
            uniformValue = getUniformValue("i4v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 81;
            uniformValue = getUniformValue("i4v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 82;

            elementLen = 1;
            gl.uniform1fv(uniform1fv, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("f1v[0]");
            if (!compareUniformValues(testArray[0], uniformValue))
                return 83;
            uniformValue = getUniformValue("f1v[1]");
            if (!compareUniformValues(testArray[1], uniformValue))
                return 84;
            uniformValue = getUniformValue("f1v[2]");
            if (!compareUniformValues(testArray[2], uniformValue))
                return 85;

            elementLen = 2;
            gl.uniform2fv(uniform2fv, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("f2v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 86;
            uniformValue = getUniformValue("f2v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 87;
            uniformValue = getUniformValue("f2v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 88;

            elementLen = 3;
            gl.uniform3fv(uniform3fv, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("f3v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 89;
            uniformValue = getUniformValue("f3v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 90;
            uniformValue = getUniformValue("f3v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 91;

            elementLen = 4;
            gl.uniform4fv(uniform4fv, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("f4v[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 92;
            uniformValue = getUniformValue("f4v[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 93;
            uniformValue = getUniformValue("f4v[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 94;

            elementLen = 2 * 2;
            gl.uniformMatrix2fv(uniformMatrix2fv, false, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix2[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 95;
            uniformValue = getUniformValue("matrix2[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 96;
            uniformValue = getUniformValue("matrix2[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 97;

            gl.uniformMatrix2fv(uniformMatrix2fv, true, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix2[0]");
            if (!compareUniformSubArraysTransposed(0 * elementLen, elementLen, testArray, uniformValue))
                return 98;
            uniformValue = getUniformValue("matrix2[1]");
            if (!compareUniformSubArraysTransposed(1 * elementLen, elementLen, testArray, uniformValue))
                return 99;
            uniformValue = getUniformValue("matrix2[2]");
            if (!compareUniformSubArraysTransposed(2 * elementLen, elementLen, testArray, uniformValue))
                return 100;

            elementLen = 3 * 3;
            gl.uniformMatrix3fv(uniformMatrix3fv, false, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix3[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 101;
            uniformValue = getUniformValue("matrix3[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 102;
            uniformValue = getUniformValue("matrix3[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 103;

            gl.uniformMatrix3fv(uniformMatrix3fv, true, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix3[0]");
            if (!compareUniformSubArraysTransposed(0 * elementLen, elementLen, testArray, uniformValue))
                return 104;
            uniformValue = getUniformValue("matrix3[1]");
            if (!compareUniformSubArraysTransposed(1 * elementLen, elementLen, testArray, uniformValue))
                return 105;
            uniformValue = getUniformValue("matrix3[2]");
            if (!compareUniformSubArraysTransposed(2 * elementLen, elementLen, testArray, uniformValue))
                return 106;

            elementLen = 4 * 4;
            gl.uniformMatrix4fv(uniformMatrix4fv, false, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix4[0]");
            if (!compareUniformSubArrays(0 * elementLen, elementLen, testArray, uniformValue))
                return 107;
            uniformValue = getUniformValue("matrix4[1]");
            if (!compareUniformSubArrays(1 * elementLen, elementLen, testArray, uniformValue))
                return 108;
            uniformValue = getUniformValue("matrix4[2]");
            if (!compareUniformSubArrays(2 * elementLen, elementLen, testArray, uniformValue))
                return 109;

            gl.uniformMatrix4fv(uniformMatrix4fv, true, constructTestArrayFloat(arrayElements * elementLen));

            uniformValue = getUniformValue("matrix4[0]");
            if (!compareUniformSubArraysTransposed(0 * elementLen, elementLen, testArray, uniformValue))
                return 110;
            uniformValue = getUniformValue("matrix4[1]");
            if (!compareUniformSubArraysTransposed(1 * elementLen, elementLen, testArray, uniformValue))
                return 111;
            uniformValue = getUniformValue("matrix4[2]");
            if (!compareUniformSubArraysTransposed(2 * elementLen, elementLen, testArray, uniformValue))
                return 112;

            gl.uniform1i(uniformSampler, 1);
            uniformValue = gl.getUniform(shaderProgram, uniformSampler);
            if (!compareUniformValues(1, uniformValue))
                return 113;

            constructTestArrayJS(3);
            var uniformLocation = gl.getUniformLocation(shaderProgram, "uStruct.first");
            gl.uniform3f(uniformLocation, testArray[0], testArray[1], testArray[2]);
            uniformValue = gl.getUniform(shaderProgram, uniformLocation);
            if (!compareUniformArrays(testArray, uniformValue))
                return 114;

            uniformLocation = gl.getUniformLocation(shaderProgram, "uStruct.second");
            gl.uniform1i(uniformLocation, fillValue);
            uniformValue = gl.getUniform(shaderProgram, uniformLocation);
            if (!compareUniformValues(fillValue, uniformValue))
                return 115;
            fillValue++;

            constructTestArrayJS(3);
            uniformLocation = gl.getUniformLocation(shaderProgram, "uStructv[0].first");
            gl.uniform3f(uniformLocation, testArray[0], testArray[1], testArray[2]);
            uniformValue = gl.getUniform(shaderProgram, uniformLocation);
            if (!compareUniformArrays(testArray, uniformValue))
                return 116;

            uniformLocation = gl.getUniformLocation(shaderProgram, "uStructv[0].second");
            gl.uniform1i(uniformLocation, fillValue);
            uniformValue = gl.getUniform(shaderProgram, uniformLocation);
            if (!compareUniformValues(fillValue, uniformValue))
                return 117;
            fillValue++;
        }
    } catch(e) {
        console.log("initializeGL(): FAILURE!");
        console.log(""+e);
        console.log(""+e.message);
        initStatus = -1;
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
                                  void main() { \
                                      gl_Position = vec4(a_position, 0.0, 1.0); \
                                  }", gl.VERTEX_SHADER);
    fragmentShader = compileShader("struct TestStruct \
                                    { \
                                        highp vec3 first; \
                                        int second; \
                                    }; \
                                    uniform highp float inactiveUniform; \
                                    uniform highp float testUniform; \
                                    uniform int i1; \
                                    uniform ivec2 i2; \
                                    uniform ivec3 i3; \
                                    uniform ivec4 i4; \
                                    uniform highp float f1; \
                                    uniform highp vec2 f2; \
                                    uniform highp vec3 f3; \
                                    uniform highp vec4 f4; \
                                    uniform bool b1; \
                                    uniform bvec2 b2; \
                                    uniform bvec3 b3; \
                                    uniform bvec4 b4; \
                                    uniform int i1v[3]; \
                                    uniform ivec2 i2v[3]; \
                                    uniform ivec3 i3v[3]; \
                                    uniform ivec4 i4v[3]; \
                                    uniform highp float f1v[3]; \
                                    uniform highp vec2 f2v[3]; \
                                    uniform highp vec3 f3v[3]; \
                                    uniform highp vec4 f4v[3]; \
                                    uniform bool b1v[3]; \
                                    uniform bvec2 b2v[3]; \
                                    uniform bvec3 b3v[3]; \
                                    uniform bvec4 b4v[3]; \
                                    uniform highp mat2 matrix2[3]; \
                                    uniform highp mat3 matrix3[3]; \
                                    uniform highp mat4 matrix4[3]; \
                                    uniform sampler2D uSampler; \
                                    uniform TestStruct uStruct; \
                                    uniform TestStruct uStructv[3]; \
                                    void main() { \
                                        int iValue = i1 + i2.x + i3.y + i4.z; \
                                        highp float fValue = f1 + f2.x + f3.y + f4.z; \
                                        if (b1 || b2.x || b3.y || b4.z) \
                                            iValue++; \
                                        fValue += uStruct.first.x; \
                                        iValue += uStruct.second; \
                                        for (int i = 0; i < 3; i++) { \
                                            iValue += i1v[i]; \
                                            iValue += i2v[i].x; \
                                            iValue += i3v[i].y; \
                                            iValue += i4v[i].z; \
                                            fValue += f1v[i]; \
                                            fValue += f2v[i].x; \
                                            fValue += f3v[i].y; \
                                            fValue += f4v[i].z; \
                                            fValue += matrix2[i][0][0]; \
                                            fValue += matrix3[i][1][1]; \
                                            fValue += matrix4[i][2][2]; \
                                            if (b1v[i] || b2v[i].x || b3v[i].y || b4v[i].z) \
                                                iValue++; \
                                            fValue += uStructv[i].first.x; \
                                            iValue += uStructv[i].second; \
                                        } \
                                        highp vec4 texelColor = texture2D(uSampler, vec2(fValue, fValue)); \
                                        if (iValue > 30) { \
                                            gl_FragColor = vec4(fValue, testUniform, texelColor.b, 1.0); \
                                        } else { \
                                            gl_FragColor = texelColor; \
                                        } \
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
