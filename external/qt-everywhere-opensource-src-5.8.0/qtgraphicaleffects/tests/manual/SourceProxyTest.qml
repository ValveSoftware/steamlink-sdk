/****************************************************************************
**
** Copyright (C) 2015 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

import QtGraphicalEffects.private 1.0
import QtQuick 2.4

Rectangle {

    Rectangle {
        width: parent.width
        height: 1
        color: "lightsteelblue"
    }

    id: root

    height: 40
    width: parent.width

    property alias proxyInterpolation: proxy.interpolation
    property bool proxyPadding: false

    property string sourcing; // "layered", "shadersource", "none";

    property bool smoothness: true
    property bool padding: false

    property alias label: text.text


    property bool expectProxy: false

    color: proxy.active ? "darkred" : "darkblue"

    Text {
        id: text
        color: "white"
        font.pixelSize: 14
        font.bold: true

        anchors.centerIn: parent

        layer.enabled: root.sourcing == "layered"
        layer.smooth: root.smoothness
        layer.sourceRect: padding ? Qt.rect(-1, -1, text.width, text.height) : Qt.rect(0, 0, 0, 0);
    }

    ShaderEffectSource {
        id: shaderSource
        sourceItem: text
        smooth: root.smoothness
        sourceRect: padding ? Qt.rect(-1, -1, text.width, text.height) : Qt.rect(0, 0, 0, 0);
    }

    SourceProxy {
        id: proxy
        input: sourcing == "shadersource" ? shaderSource : (root.sourcing == "layered" ? text : null);
        visible: false
        sourceRect: proxyPadding ? Qt.rect(-1, -1, text.width, text.height) : Qt.rect(0, 0, 0, 0);
    }

    Text {
        id: autoConfLabel;
        // This will be shown when the source is a layer which has different
        // attributes set than what the source proxy expects. The source proxy
        // will then configure the layer.
        color: "#00ff00"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter;
        text: "(configured)"
        font.pixelSize: 12
        font.bold: true
        visible: root.expectProxy != proxy.active && !proxy.active && root.sourcing == "layered";
    }

    Text {
        color: "red"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter;
        rotation: 45
        text: "FAIL"
        font.pixelSize: 12
        font.bold: true
        visible: root.expectProxy != proxy.active && !autoConfLabel.visible
    }

}
