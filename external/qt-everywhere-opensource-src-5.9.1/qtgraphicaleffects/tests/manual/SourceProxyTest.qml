/****************************************************************************
**
** Copyright (C) 2017 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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
