/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtQuick.Controls.Styles.Android 1.0

Drawable {
    id: root

    implicitWidth: Math.max(image.implicitWidth, styleDef.width || 0)
    implicitHeight: Math.max(image.implicitHeight, styleDef.height || 0)

    Image {
        id: image
        anchors.fill: parent
        fillMode: Image.TileHorizontally
        source: AndroidStyle.filePath(styleDef.path)

        layer.enabled: !!styleDef && !!styleDef.tintList
        layer.effect: ShaderEffect {
            property variant source: image
            property color color: AndroidStyle.colorValue(styleDef.tintList[state])
            state: {
                var states = []
                if (pressed) states.push("PRESSED")
                if (enabled) states.push("ENABLED")
                if (focused) states.push("FOCUSED")
                if (selected) states.push("SELECTED")
                if (window_focused) states.push("WINDOW_FOCUSED")
                if (!states.length)
                    states.push("EMPTY")
                return states.join("_") + "_STATE_SET"
            }
            // QtGraphicalEffects/ColorOverlay:
            fragmentShader: "
                varying mediump vec2 qt_TexCoord0;
                uniform highp float qt_Opacity;
                uniform lowp sampler2D source;
                uniform highp vec4 color;
                void main() {
                    highp vec4 pixelColor = texture2D(source, qt_TexCoord0);
                    gl_FragColor = vec4(mix(pixelColor.rgb/max(pixelColor.a, 0.00390625), color.rgb/max(color.a, 0.00390625), color.a) * pixelColor.a, pixelColor.a) * qt_Opacity;
                }
            "
        }
    }
}
