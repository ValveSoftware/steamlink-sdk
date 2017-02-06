/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.2
import QtQuick.Controls 1.1

Rectangle {
    id: view
    color: darkBackground ? "transparent" : lightBackgroundColor

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            stackView.pop();
            event.accepted = true;
        }
    }

    property bool darkBackground: true

    property Component control
    property Component customizer

    property alias controlItem: controlLoader.item
    property alias customizerItem: customizerLoader.item

    property bool isCustomizerVisible: false

    property real margin: root.toPixels(0.05)

    property rect controlBounds: Qt.rect(largestControlItem.x + controlBoundsItem.x,
        largestControlItem.y + controlBoundsItem.y, controlBoundsItem.width, controlBoundsItem.height)

    Item {
        id: largestControlItem
        x: margin
        y: margin
        width: isCustomizerVisible ? widthWhenCustomizing : widthWhenNotCustomizing
        height: isCustomizerVisible ? heightWhenCustomizing : heightWhenNotCustomizing

        readonly property real widthWhenCustomizing: (!isScreenPortrait ? parent.width / 2 : parent.width) - margin * 2
        readonly property real heightWhenCustomizing: (isScreenPortrait ? parent.height / 2 : parent.height - toolbar.height) - margin * 2
        readonly property real widthWhenNotCustomizing: parent.width - margin * 2
        readonly property real heightWhenNotCustomizing: parent.height - toolbar.height - margin * 2

        Item {
            id: controlBoundsItem
            x: parent.width / 2 - controlBoundsItem.width / 2
            y: customizer && customizerItem.visible ? 0 : (isScreenPortrait ? (parent.height / 2) - (controlBoundsItem.height / 2) : 0)
            width: Math.min(parent.widthWhenCustomizing, parent.widthWhenNotCustomizing)
            height: Math.min(parent.heightWhenCustomizing, parent.heightWhenNotCustomizing)

            Behavior on x {
                id: controlXBehavior
                enabled: false
                NumberAnimation {}
            }

            Behavior on y {
                id: controlYBehavior
                enabled: false
                NumberAnimation {}
            }

            Loader {
                id: controlLoader
                sourceComponent: control
                anchors.centerIn: parent

                property alias view: view
            }
        }
    }

    Flickable {
        id: flickable
        // Hide the customizer on the right of the screen if it's not visible.
        x: (isScreenPortrait ? 0 : (isCustomizerVisible ? largestControlItem.x + largestControlItem.width + margin : view.width)) + margin
        y: (isScreenPortrait ? largestControlItem.y + largestControlItem.height : 0) + margin
        width: largestControlItem.width
        height: parent.height - y - toolbar.height - margin
        anchors.leftMargin: margin
        anchors.rightMargin: margin
        visible: customizerLoader.opacity > 0

        flickableDirection: Flickable.VerticalFlick

        clip: true
        contentWidth: width
        contentHeight: customizerLoader.height

        Behavior on x {
            id: flickableXBehavior
            enabled: false
            NumberAnimation {}
        }

        Behavior on y {
            id: flickableYBehavior
            enabled: false
            NumberAnimation {}
        }

        Loader {
            id: customizerLoader
            sourceComponent: customizer
            opacity: 0
            width: flickable.width

            property alias view: view

            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }
    }

    ControlViewToolbar {
        id: toolbar

        onCustomizeClicked: {
            controlXBehavior.enabled = !isScreenPortrait;
            controlYBehavior.enabled = isScreenPortrait;

            isCustomizerVisible = !isCustomizerVisible;

            if (isScreenPortrait) {
                flickableXBehavior.enabled = false;
                flickableYBehavior.enabled = true;
            } else {
                flickableXBehavior.enabled = true;
                flickableYBehavior.enabled = false;
            }

            customizerLoader.opacity = isCustomizerVisible ? 1 : 0;
        }
    }

    FlickableMoreIndicator {
        flickable: flickable
        atTop: true
        gradientColor: view.darkBackground ? darkBackgroundColor : lightBackgroundColor
    }

    FlickableMoreIndicator {
        flickable: flickable
        atTop: false
        gradientColor: view.darkBackground ? darkBackgroundColor : lightBackgroundColor
    }
}
