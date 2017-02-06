/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

function restart() {
    for (var i = 0; i < initState.length; i++)
        currentState[i] = initState[i].slice();
    undoStack = [];
}

function isValidPosition() {
    var x = _event.data.x;
    var y = _event.data.y;
    if (x < 0 || x >= initState.length)
        return false;
    if (y < 0 || y >= initState.length)
        return false;
    if (initState[x][y] !== 0)
        return false;
    return true;
}

function calculateCurrentState() {
    if (isValidPosition() === false)
        return;
    var x = _event.data.x;
    var y = _event.data.y;
    var currentValue = currentState[x][y];
    if (currentValue === initState.length)
        currentValue = 0;
    else
        currentValue += 1;
    currentState[x][y] = currentValue;
    undoStack.push([x, y]);
}

function isOK(numbers) {
    var temp = [];
    for (var i = 0; i < numbers.length; i++) {
        var currentValue = numbers[i];
        if (currentValue === 0)
            return false;
        if (temp.indexOf(currentValue) >= 0)
            return false;
        temp.push(currentValue);
    }
    return true;
}

function isSolved() {
    for (var i = 0; i < currentState.length; i++) {
        if (!isOK(currentState[i]))
            return false;

        var column = [];
        var square = [];
        for (var j = 0; j < currentState[i].length; j++) {
            column.push(currentState[j][i]);
            square.push(currentState[Math.floor(i / 3) * 3 + Math.floor(j / 3)]
                                    [i % 3 * 3 + j % 3]);
        }

        if (!isOK(column))
            return false;
        if (!isOK(square))
            return false;
    }
    return true;
}

function undo() {
    if (!undoStack.length)
        return;

    var lastMove = undoStack.pop();
    var x = lastMove[0];
    var y = lastMove[1];
    var currentValue = currentState[x][y];
    if (currentValue === 0)
        currentValue = initState.length;
    else
        currentValue -= 1;
    currentState[x][y] = currentValue;
}
