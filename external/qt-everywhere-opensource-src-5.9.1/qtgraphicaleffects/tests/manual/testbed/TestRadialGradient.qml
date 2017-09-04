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
        id: maskImage
        source: "images/butterfly.png"
    }

    RadialGradient {
        id: effect
        anchors.fill: parent
        cached: cachedCheckBox.selected
        visible: enabledCheckBox.selected
        source: maskCheckBox.selected ?  maskImage : undefined
        horizontalOffset: (offsetPicker.xValue - 0.5) * width
        verticalOffset: (offsetPicker.yValue - 0.5) * height
        horizontalRadius: horizontalRadiusSlider.value
        verticalRadius: verticalRadiusSlider.value
        angle: angleSlider.value
        gradient: Gradient {
            GradientStop {position: 0.0; color: gradientBeginColorSlider.color}
            GradientStop {position: 1.0; color: gradientEndColorSlider.color}
        }
    }

    PositionPicker {
        id: offsetPicker
        xValue: 0.5
        yValue: 0.5
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
                id: horizontalRadiusSlider
                minimum: 0
                maximum: effect.width
                value: effect.width / 2
                caption: "horizontalRadius"
            }
            Slider {
                id: verticalRadiusSlider
                minimum: 0
                maximum: effect.height
                value: effect.height / 2
                caption: "verticalRadius"
            }
            Slider {
                id: angleSlider
                minimum: 0
                maximum: 360
                value: 0
                caption: "angle"
            }
            CheckBox {
                id: maskCheckBox
                caption: "Use Mask"
            }
        },

        Control {
            caption: "gradient begin color"
            ColorPicker {
                id: gradientBeginColorSlider
                hue: 0.67
                saturation: 1.0
                lightness: 0.5
            }
        },

        Control {
            caption: "gradient end color"
            ColorPicker {
                id: gradientEndColorSlider
                hue: 0.5
                saturation: 1.0
                lightness: 0.5
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
            }
        }
    ]
}
