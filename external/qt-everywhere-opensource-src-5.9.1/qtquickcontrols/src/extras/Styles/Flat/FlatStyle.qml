/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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
pragma Singleton

import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0

QtObject {
    readonly property string fontFamily: __openSansRegularLoader.name

    readonly property color backgroundColor: "#ffffff"
    readonly property color defaultTextColor: "#333333"
    readonly property color styleColor: "#5caa15"
    readonly property color strokeColor: "#328930"
    readonly property color hoveredColor: "#7dbb44"
    readonly property color focusedColor: "#4da6d8"
    readonly property color focusedTextColor: "#1288cb"
    readonly property color focusedAndPressedColor: "#4595c2"
    readonly property color focusedStrokeColor: "#2f86bb"
    readonly property color focusedAndPressedStrokeColor: "#266b96"
    readonly property color checkedFocusedAndPressedColor: "#3a7ca2"
    readonly property color checkedAndPressedColor: "#4a8811"
    readonly property color checkedAndPressedColorAlt: "#3c6f0e"
    readonly property color pressedColor: "#457f10"
    readonly property color pressedColorAlt: "#539913"
    readonly property color pressedStrokeColor: "#286e26"
    readonly property color invalidColor: "#dd0000"
    readonly property color disabledColor: "#000000"
    readonly property color disabledFillColor: "#d9d9d9"
    readonly property color highlightColor: "#4da6d8"
    readonly property color selectedTextColor: "#ffffff"
    readonly property color textSelectionColor: "#ceeaff"
    readonly property color textColor: "#1a1a1a"
    readonly property color grooveColor: "#14000000"
    readonly property color lightFrameColor: "#cccccc"
    readonly property color mediumFrameColor: "#b3b3b3"
    readonly property color darkFrameColor: "#999999"
    readonly property color alphaFrameColor: "#33000000"
    readonly property color selectionHandleColor: "#0668ec"
    readonly property color flatFrameColor: "#f2f2f2"
    readonly property real disabledOpacity: 0.3
    readonly property real defaultFontSize: 13

    // 16 is the implicitHeight of text on a PC monitor. This should scale well
    // to other devices. For example, if the implicitHeight is 64 on a touch
    // device, the scale factor will be x4.
    readonly property real scaleFactor: Math.max(1, TextSingleton.implicitHeight / 16)

    readonly property real onePixel: Math.max(1, Math.round(scaleFactor * 1))
    readonly property real twoPixels: Math.max(2, Math.round(scaleFactor * 2))
    readonly property int radius: Math.round(scaleFactor * 3)

    property FontLoader __openSansDemiBoldLoader: FontLoader {
        source: "fonts/OpenSans-Semibold.ttf"
    }

    property FontLoader __openSansRegularLoader: FontLoader {
        source: "fonts/OpenSans-Regular.ttf"
    }

    property FontLoader __openSansLightLoader: FontLoader {
        source: "fonts/OpenSans-Light.ttf"
    }

    readonly property int __renderType: Text.QtRendering
}
