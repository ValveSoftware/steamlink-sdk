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
        forcedUpdateAnimationRunning: false
    }

    RecursiveBlur {
        id: effect
        loops: iterationSlider.value
        anchors.fill: imageSource
        radius: radiusSlider.value
        transparentBorder: transparentBorderCheckBox.selected
        visible: enabledCheckBox.selected
        cached: cachedCheckBox.selected
        source: imageSource
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Slider {
                id: iterationSlider
                minimum: 0
                maximum: 20
                value: 0
                integer:  true
                caption: "loops"
            }
            Slider {
                id: radiusSlider
                minimum: 0.0
                maximum: 16.0
                value: 7.5
                caption: "radius"
            }
            CheckBox {
                id: transparentBorderCheckBox
                caption: "transparentBorder"
                selected: false
            }
            ProgressBar {
                id: progressSlider
                minimum: 0
                maximum: 1.0
                value: effect.progress
                caption: "progress"
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
