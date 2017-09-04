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

    LinearGradient {
        id: effect
        anchors.fill: parent
        cached: cachedCheckBox.selected
        visible: enabledCheckBox.selected
        source: maskCheckBox.selected ? maskImage : undefined
        start: Qt.point(startPicker.xValue * width, startPicker.yValue * height)
        end: Qt.point(endPicker.xValue * width, endPicker.yValue * height)
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: gradientBeginColorSlider.color
            }
            GradientStop {
                position: 1.0
                color: gradientEndColorSlider.color
            }
        }
    }

    PositionPicker {
        id: startPicker
        xValue: 0.2
        yValue: 0.2
    }
    PositionPicker {
        id: endPicker
        xValue: 0.8
        yValue: 0.8
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Label {
                caption: "startX"
                text: startPicker.xValue.toFixed(1)
            }
            Label {
                caption: "startY"
                text: startPicker.yValue.toFixed(1)
            }
            Label {
                caption: "endX"
                text: endPicker.xValue.toFixed(1)
            }
            Label {
                caption: "endY"
                text: endPicker.yValue.toFixed(1)
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
