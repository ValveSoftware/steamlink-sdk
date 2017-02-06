/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

import QtQuick 2.0
import QtWayland.Compositor 1.0

Item {
    id: chrome
    property alias shellSurface: surfaceItem.shellSurface
    property alias surfaceItem: surfaceItem
    property alias moveItem: surfaceItem.moveItem

    x: surfaceItem.moveItem.x - surfaceItem.output.geometry.x
    y: surfaceItem.moveItem.y - surfaceItem.output.geometry.y

    ShellSurfaceItem {
        id: surfaceItem
        onSurfaceDestroyed: chrome.destroy();
    }

    onXChanged: updatePrimary()
    onYChanged: updatePrimary()
    function updatePrimary() {
        var w = surfaceItem.width
        var h = surfaceItem.height
        var area = w * h;
        var screenW = surfaceItem.output.geometry.width;
        var screenH = surfaceItem.output.geometry.height;
        var x1 = Math.max(0, x);
        var y1 = Math.max(0, y);
        var x2 = Math.min(x + w, screenW);
        var y2 = Math.min(y + h, screenH);
        var w1 = Math.max(0, x2 - x1);
        var h1 = Math.max(0, y2 - y1);
        if (w1 * h1 * 2 > area) {
            surfaceItem.setPrimary();
        }
    }
}
