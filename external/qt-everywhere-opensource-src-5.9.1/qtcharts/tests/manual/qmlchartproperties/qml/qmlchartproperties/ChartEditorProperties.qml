/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0

Flow {
    anchors.fill: parent
    property variant chart
    property variant easingTypes: [Easing.Linear, Easing.InCubic, Easing.OutSine, Easing.OutInBack]
    property int easingTypeIndex: 0
    flow: Flow.TopToBottom
    spacing: 5

    function nextEasingType() {
        easingTypeIndex++;
        return easingTypes[easingTypeIndex % easingTypes.length];
    }

    Button {
        text: "visible"
        onClicked: chart.visible = !chart.visible;
    }
    Button {
        text: "theme +"
        onClicked: chart.theme++;
    }
    Button {
        text: "theme -"
        onClicked: chart.theme--;
    }
    Button {
        text: "animation opt +"
        onClicked: chart.animationOptions++;
    }
    Button {
        text: "animation opt -"
        onClicked: chart.animationOptions--;
    }
    Button {
        text: "animation duration +"
        onClicked: chart.animationDuration = chart.animationDuration + 1000;
    }
    Button {
        text: "animation duration -"
        onClicked: chart.animationDuration = chart.animationDuration - 1000;
    }
    Button {
        text: "animation curve"
        onClicked: chart.animationEasingCurve.type = nextEasingType();
    }
    Button {
        text: "background color"
        onClicked: chart.backgroundColor = main.nextColor();
    }
    Button {
        text: "plot area color"
        onClicked: chart.plotAreaColor = main.nextColor();
    }
    Button {
        text: "drop shadow enabled"
        onClicked: chart.dropShadowEnabled = !chart.dropShadowEnabled;
    }
    Button {
        text: "roundness"
        onClicked: chart.backgroundRoundness++;
    }
    Button {
        text: "zoom +"
        onClicked: chart.zoom(2);
    }
    Button {
        text: "zoom -"
        onClicked: chart.zoom(0.5);
    }
    Button {
        text: "scroll left"
        onClicked: chart.scrollLeft(10);
    }
    Button {
        text: "scroll right"
        onClicked: chart.scrollRight(10);
    }
    Button {
        text: "scroll up"
        onClicked: chart.scrollUp(10);
    }
    Button {
        text: "scroll down"
        onClicked: chart.scrollDown(10);
    }
    Button {
        text: "title color"
        onClicked: chart.titleColor = main.nextColor();
    }
    Button {
        text: "top margin +"
        onClicked: chart.margins.top += 5;
    }
    Button {
        text: "top margin -"
        onClicked: chart.margins.top -= 5;
    }
    Button {
        text: "bottom margin +"
        onClicked: chart.margins.bottom += 5;
    }
    Button {
        text: "bottom margin -"
        onClicked: chart.margins.bottom -= 5;
    }
    Button {
        text: "left margin +"
        onClicked: chart.margins.left += 5;
    }
    Button {
        text: "left margin -"
        onClicked: chart.margins.left -= 5;
    }
    Button {
        text: "right margin +"
        onClicked: chart.margins.right += 5;
    }
    Button {
        text: "right margin -"
        onClicked: chart.margins.right -= 5;
    }
}
