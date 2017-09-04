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

    ImageSource {
        id: imageSource
        source: "images/butterfly.png"
        anchors.centerIn: parent
    }

    InnerShadow {
        id: effect
        anchors.fill: imageSource
        horizontalOffset: (offsetPicker.xValue - 0.5) * width
        verticalOffset: (offsetPicker.yValue - 0.5) * height
        samples: samplesSlider.value
        radius: radiusSlider.value
        spread: spreadSlider.value
        color: colorPicker.color
        visible: enabledCheckBox.selected
        cached: cachedCheckBox.selected
        fast: fastCheckBox.selected
        source: imageSource
    }

    PositionPicker {
        id: offsetPicker
        xValue: 0.50
        yValue: 0.51
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Label {
                caption: "horizontalOffset"
                text: effect.horizontalOffset.toFixed(1)
            }
            Label {
                caption: "verticalOffset"
                text: effect.verticalOffset.toFixed(1)
            }
            Slider {
                id: radiusSlider
                minimum: 0
                maximum: 16
                value: 8
                caption: "radius"
            }
            Slider {
                id: samplesSlider
                minimum: 0
                maximum: 32
                value: 32
                integer: true
                caption: "samples"
            }
            Slider {
                id: spreadSlider
                minimum: 0
                maximum: 1.0
                value: 0.0
                caption: "spread"
            }
            CheckBox {
                id: fastCheckBox
                caption: "fast"
                selected: false
            }
        },

        Control {
            caption: "color"
            ColorPicker {
                id: colorPicker
                hue: 0
                saturation: 0
                lightness: 0
                alpha: 0.75
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
            CheckBox {
                id: updateCheckBox
                caption: "animated"
                selected: false
            }
            BGColorPicker {
                id: bgColorPicker
                color: "#ababab"
            }
        }
    ]
}
