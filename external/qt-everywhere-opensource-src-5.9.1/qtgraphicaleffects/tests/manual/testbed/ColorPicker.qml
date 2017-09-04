/*****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Add-On Graphical Effects module.
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
*****************************************************************************/

import QtQuick 2.0

Item {
    id: root
    property color color: Qt.hsla(hue, saturation, lightness, alpha)
    property alias hue: hueSlider.value
    property alias saturation: saturationSlider.value
    property alias lightness: lightnessSlider.value
    property alias alpha: alphaSlider.value
    property bool showAlphaSlider: true

    width: parent.width
    height: 100

    Image {
        anchors.fill: map
        source: "images/background.png"
        fillMode: Image.Tile
    }

    Rectangle {
        id: colorBox
        anchors.fill: map
        color: root.color
    }

    ShaderEffect {
        id: map
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.top: parent.top
        anchors.topMargin: 5
        width: 68
        height: width
        opacity: 0.01
        property real hue: root.hue

        fragmentShader: "
        varying mediump vec2 qt_TexCoord0;
        uniform highp float qt_Opacity;
        uniform highp float hue;

        highp float hueToIntensity(highp float v1, highp float v2, highp float h) {
            h = fract(h);
            if (h < 1.0 / 6.0)
                return v1 + (v2 - v1) * 6.0 * h;
            else if (h < 1.0 / 2.0)
                return v2;
            else if (h < 2.0 / 3.0)
                return v1 + (v2 - v1) * 6.0 * (2.0 / 3.0 - h);

            return v1;
        }

        highp vec3 HSLtoRGB(highp vec3 color) {
            highp float h = color.x;
            highp float l = color.z;
            highp float s = color.y;

            if (s < 1.0 / 256.0)
                return vec3(l, l, l);

            highp float v1;
            highp float v2;
            if (l < 0.5)
                v2 = l * (1.0 + s);
            else
                v2 = (l + s) - (s * l);

            v1 = 2.0 * l - v2;

            highp float d = 1.0 / 3.0;
            highp float r = hueToIntensity(v1, v2, h + d);
            highp float g = hueToIntensity(v1, v2, h);
            highp float b = hueToIntensity(v1, v2, h - d);
            return vec3(r, g, b);
        }

        void main() {
            lowp vec4 c = vec4(1.0);
            c.rgb = HSLtoRGB(vec3(hue, 1.0 - qt_TexCoord0.t, qt_TexCoord0.s));
            gl_FragColor = c * qt_Opacity;
        }
        "

        MouseArea {
            id: mapMouseArea
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            onPositionChanged: {
                if (pressed) {
                    var xx = Math.max(0, Math.min(mouse.x, parent.width))
                    var yy = Math.max(0, Math.min(mouse.y, parent.height))
                    root.saturation = 1.0 - yy / parent.height
                    root.lightness = xx / parent.width
                }
            }
            onPressed: positionChanged(mouse)

            onEntered: map.opacity = 1
            onReleased: {
                if (mouse.x < 0 || mouse.x > parent.width || mouse.y < 0 || mouse.y > parent.height) {
                    map.opacity = 0.01;
                }
            }
            onExited: {
                if (!pressed) {
                    map.opacity = 0.01;
                }
            }
        }

        Image {
            id: crosshair
            source: "images/slider_handle.png"
            x: root.lightness * parent.width - width / 2
            y: (1.0 - root.saturation) * parent.height - height / 2
        }
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right

        ColorSlider {
            id: hueSlider
            minimum: 0.0
            maximum: 1.0
            value: 0.5
            caption: "H"
            trackItem: Rectangle {
                width: parent.height
                height: parent.width - 10
                color: "red"
                rotation: -90
                transformOrigin: Item.TopLeft
                y: width
                x: 5
                gradient: Gradient {
                    GradientStop {position: 0.000; color: Qt.rgba(1, 0, 0, 1)}
                    GradientStop {position: 0.167; color: Qt.rgba(1, 1, 0, 1)}
                    GradientStop {position: 0.333; color: Qt.rgba(0, 1, 0, 1)}
                    GradientStop {position: 0.500; color: Qt.rgba(0, 1, 1, 1)}
                    GradientStop {position: 0.667; color: Qt.rgba(0, 0, 1, 1)}
                    GradientStop {position: 0.833; color: Qt.rgba(1, 0, 1, 1)}
                    GradientStop {position: 1.000; color: Qt.rgba(1, 0, 0, 1)}
                }
            }
        }

        ColorSlider {
            id: saturationSlider
            minimum: 0.0
            maximum: 1.0
            value: 1.0
            caption: "S"
            handleOpacity: 1.5 - map.opacity
            trackItem: Rectangle {
                width: parent.height
                height: parent.width - 10
                color: "red"
                rotation: -90
                transformOrigin: Item.TopLeft
                y: width
                x: 5
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.hsla(root.hue, 0.0, root.lightness, 1.0) }
                    GradientStop { position: 1; color: Qt.hsla(root.hue, 1.0, root.lightness, 1.0) }
                }
            }
        }

        ColorSlider {
            id: lightnessSlider
            minimum: 0.0
            maximum: 1.0
            value: 0.5
            caption: "L"
            handleOpacity: 1.5 - map.opacity
            trackItem: Rectangle {
                width: parent.height
                height: parent.width - 10
                color: "red"
                rotation: -90
                transformOrigin: Item.TopLeft
                y: width
                x: 5
                gradient: Gradient {
                    GradientStop { position: 0; color: 'black' }
                    GradientStop { position: 0.5; color: Qt.hsla(root.hue, root.saturation, 0.5, 1.0) }
                    GradientStop { position: 1; color: 'white' }
                }
            }
        }

        ColorSlider {
            id: alphaSlider
            minimum: 0.0
            maximum: 1.0
            value: 1.0
            caption: "A"
            opacity: showAlphaSlider ? 1.0 : 0.0
            trackItem:Item {
                anchors.fill: parent
                Image {
                    anchors {fill: parent; leftMargin: 5; rightMargin: 5}
                    source: "images/background.png"
                    fillMode: Image.TileHorizontally
                }
                Rectangle {
                    width: parent.height
                    height: parent.width - 10
                    color: "red"
                    rotation: -90
                    transformOrigin: Item.TopLeft
                    y: width
                    x: 5
                    gradient: Gradient {
                        GradientStop { position: 0; color: "transparent" }
                        GradientStop { position: 1; color: Qt.hsla(root.hue, root.saturation, root.lightness, 1.0) }
                    }
                }
            }
        }

        Label {
            caption: "ARGB"
            text: "#" + ((Math.ceil(root.alpha * 255) + 256).toString(16).substr(1, 2) + root.color.toString().substr(1, 6)).toUpperCase();
        }
    }
}
