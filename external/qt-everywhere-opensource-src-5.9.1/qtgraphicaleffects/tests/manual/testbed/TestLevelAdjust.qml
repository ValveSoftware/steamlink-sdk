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
    }

    LevelAdjust {
        id: effect
        anchors.fill: imageSource
        visible: enabledCheckBox.selected
        cached: cachedCheckBox.selected
        source: imageSource

        minimumInput: Qt.rgba(redInput.blackPointValue + valueInput.blackPointValue * (redInput.whitePointValue - redInput.blackPointValue), greenInput.blackPointValue + valueInput.blackPointValue * (greenInput.whitePointValue - greenInput.blackPointValue), blueInput.blackPointValue + valueInput.blackPointValue * (blueInput.whitePointValue - blueInput.blackPointValue), alphaInput.blackPointValue)
        maximumInput: Qt.rgba(redInput.whitePointValue - (1.0 - valueInput.whitePointValue) * (redInput.whitePointValue - redInput.blackPointValue), greenInput.whitePointValue - (1.0 - valueInput.whitePointValue) * (greenInput.whitePointValue - greenInput.blackPointValue), blueInput.whitePointValue - (1.0 - valueInput.whitePointValue) * (blueInput.whitePointValue - blueInput.blackPointValue), alphaInput.whitePointValue)
        minimumOutput: Qt.rgba(redOutput.blackPointValue + valueOutput.blackPointValue * (redOutput.whitePointValue - redOutput.blackPointValue), greenOutput.blackPointValue + valueOutput.blackPointValue * (greenOutput.whitePointValue - greenOutput.blackPointValue), blueOutput.blackPointValue + valueOutput.blackPointValue * (blueOutput.whitePointValue - blueOutput.blackPointValue), alphaOutput.blackPointValue)
        maximumOutput: Qt.rgba(redOutput.whitePointValue - (1.0 - valueOutput.whitePointValue) * (redOutput.whitePointValue - redOutput.blackPointValue), greenOutput.whitePointValue - (1.0 - valueOutput.whitePointValue) * (greenOutput.whitePointValue - greenOutput.blackPointValue), blueOutput.whitePointValue - (1.0 - valueOutput.whitePointValue) * (blueOutput.whitePointValue - blueOutput.blackPointValue), alphaOutput.whitePointValue)

        gamma: Qt.vector3d((redInput.gamma * valueInput.gamma), (greenInput.gamma * valueInput.gamma), (blueInput.gamma * valueInput.gamma))
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "RGB"
            LevelSlider {
                id: valueInput
                caption: "Input"
            }
            LevelSlider {
                id: valueOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "Red"
            __hide: true
            LevelSlider {
                id: redInput
                caption: "Input"
            }
            LevelSlider {
                id: redOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "Green"
            __hide: true
            LevelSlider {
                id: greenInput
                caption: "Input"
            }
            LevelSlider {
                id: greenOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "Blue"
            __hide: true
            LevelSlider {
                id: blueInput
                caption: "Input"
            }
            LevelSlider {
                id: blueOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "Alpha"
            __hide: true
            LevelSlider {
                id: alphaInput
                showMidPoint: false
                caption: "Input"
            }
            LevelSlider {
                id: alphaOutput
                showMidPoint: false
                caption: "Output"
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
