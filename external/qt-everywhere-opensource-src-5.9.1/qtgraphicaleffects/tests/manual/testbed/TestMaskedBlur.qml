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
    }

    Item {
        id: maskSource
        anchors.fill: imageSource
        visible: false
        Rectangle {
            rotation: -90
            anchors.fill: parent
            gradient: Gradient {
                     GradientStop { position: 0.2; color: Qt.rgba(maskAlphaBeginSlider.value, maskAlphaBeginSlider.value, maskAlphaBeginSlider.value, maskAlphaBeginSlider.value) }
                     GradientStop { position: 0.5; color: Qt.rgba(maskAlphaEndSlider.value, maskAlphaEndSlider.value, maskAlphaEndSlider.value, maskAlphaEndSlider.value) }
            }
        }
    }

    MaskedBlur {
        id: effect
        anchors.fill: imageSource
        radius: radiusSlider.value
        samples: samplesSlider.value
        transparentBorder: transparentBorderCheckBox.selected
        visible: enabledCheckBox.selected
        cached: cachedCheckBox.selected
        fast: fastCheckBox.selected
        source: imageSource
        maskSource: maskSource
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Slider {
                id: radiusSlider
                minimum: 0.0
                maximum: 32.0
                value: 16.0
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
            CheckBox {
                id: transparentBorderCheckBox
                caption: "transparentBorder"
                selected: false
            }
            CheckBox {
                id: fastCheckBox
                caption: "fast"
                selected: false
            }
        },
        Control {
            caption: "maskSource gradient"
            Slider {
                id: maskAlphaBeginSlider
                minimum: 0.0
                maximum: 1.0
                value: 1.0
                caption: "opacity begin"
            }
            Slider {
                id: maskAlphaEndSlider
                minimum: 0.0
                maximum: 1.0
                value: 0.0
                caption: "opacity end"
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
