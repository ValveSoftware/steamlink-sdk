/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

import QtQuick 2.0
import "../../../src/effects"

TestCaseTemplate {

    RectangularGlow {
        id: effect
        anchors.fill: rectSource
        color: colorPicker.color
        spread: spreadSlider.value
        glowRadius: sizeSlider.value
        visible: enabledCheckBox.selected
        opacity: opacitySlider.value
        cornerRadius: radiusSlider.value
        cached: cachedCheckBox.selected
    }

    Rectangle {
        id: rectSource
        visible: true
        anchors.centerIn: parent
        width: Math.round(parent.width / 1.5)
        height: Math.round(parent.height / 2)
        radius: 25
        color: "black"
        smooth: true
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Slider {
                id: spreadSlider
                caption: "spread"
                minimum: 0.0
                maximum: 1.0
                value: 0.5
            }
            Slider {
                id: sizeSlider
                minimum: 0
                maximum: rectSource.width / 2.0
                value: 10.0
                caption: "glowRadius"
                onValueChanged: radiusSlider.value = Math.max(0, Math.min(radiusSlider.value, radiusSlider.maximum))
            }
            Slider {
                id: radiusSlider
                minimum: 0
                maximum: Math.min(effect.width, effect.height) / 2 + effect.glowRadius;
                caption: "cornerRadius"
                value: rectSource.radius + effect.glowRadius
            }
            Slider {
                id: opacitySlider
                minimum: 0
                maximum: 1.0
                value: 1.0
                caption: "opacity"
            }
        },

        Control {
            caption: "color"
            ColorPicker {
                id: colorPicker
                hue: 0
                saturation: 1
                lightness: 1
            }
        },

        Control {
            caption: "advanced"
            last: true
            Label {
                caption: "Effect size"
                text: effect.width + "x" + effect.height
            }
            Label {
                caption: "FPS"
                text: fps
            }
            CheckBox {
                id: cachedCheckBox
                caption: "cached"
            }
            CheckBox {
                id: enabledCheckBox
                caption: "enabled"
            }
            BGColorPicker {
                id: bgColorPicker
            }
        }
    ]
}
