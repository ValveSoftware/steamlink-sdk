/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

var windowList = null;
var indexes = null;

function relayout() {
    if (windowList === null || windowList.length == 0)
        return;

    var dim = Math.ceil(Math.sqrt(windowList.length));

    var cols = dim;
    var rows = Math.ceil(windowList.length / cols);

    var w = root.width / dim;
    var h = root.height / rows;

    var i;
    var ix = 0;
    var iy = 0;
    var lastDim = 1;

    indexes = new Array(dim * dim);

    for (i = 0; i < windowList.length; ++i) {
        if (i > 0) {
            var currentDim = Math.ceil(Math.sqrt(i + 1));
            if (currentDim == lastDim) {
                if (iy < currentDim - 1) {
                    ++iy;
                    if (iy == currentDim - 1)
                        ix = 0;
                } else {
                    ++ix;
                }
            } else {
                iy = 0;
                ix = currentDim - 1;
            }
            lastDim = currentDim;
        }

        indexes[iy * dim + ix] = i;
        windowList[i].index = iy * dim + ix;

        var cx = (ix + 0.5) * w;
        var cy = (iy + 0.5) * h;

        windowList[i].targetScale = 0.98 * Math.min(w / windowList[i].width, h / windowList[i].height);

        windowList[i].targetX = (cx - windowList[i].width / 2);
        windowList[i].targetY = (cy - windowList[i].height / 2);
    }
}

function addWindow(window)
{
    if (windowList == null)
        windowList = new Array(0);

    windowList.push(window);
    relayout();
}

function removeWindow(window)
{
    var i;
    for (i = 0; i < windowList.length; ++i) {
        if (windowList[i] == window)
            break;
    }

    var index = windowList[i].index;
    var dim = Math.ceil(Math.sqrt(windowList.length));
    var maxY = Math.floor((windowList.length-1) / dim);

    var shrinking = Math.ceil(Math.sqrt(windowList.length - 1)) != dim;

    while (true) {
        var ix = index % dim;
        var iy = Math.floor(index / dim);

        if (shrinking) {
            if (iy > 0)
                --iy;
            else if (++ix == dim)
                break;
        } else {
            if (iy < maxY) {
                if (ix > 0)
                    --ix;
                else
                    ++iy;
            } else {
                ++ix;
            }
        }

        var next = iy * dim + ix;

        var currentIndex = indexes[index];
        var nextIndex = indexes[next];

        if (nextIndex == null)
            break;

        var temp = windowList[currentIndex];
        windowList[currentIndex] = windowList[nextIndex];
        windowList[currentIndex].index = currentIndex;
        windowList[nextIndex] = temp;

        index = next;
    }

    windowList.splice(indexes[index], 1);
    relayout();
}
