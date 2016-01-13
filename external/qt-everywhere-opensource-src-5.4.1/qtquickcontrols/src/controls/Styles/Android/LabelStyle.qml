/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles.Android 1.0

Text {
    id: root

    property var styleDef

    property bool pressed
    property bool focused
    property bool selected
    property bool window_focused

    readonly property string key: {
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

    readonly property string selection: {
        var states = []
        if (pressed) states.push("PRESSED")
        if (enabled) states.push("ENABLED")
        if (focused) states.push("FOCUSED")
        states.push("SELECTED")
        if (window_focused) states.push("WINDOW_FOCUSED")
        return states.join("_") + "_STATE_SET"
    }

    readonly property int textStyleBold: 1
    readonly property int textStyleItalic: 2

    readonly property int ellipsizeStart: 1
    readonly property int ellipsizeMiddle: 2
    readonly property int ellipsizeEnd: 3
    readonly property int ellipsizeMarquee: 4

    // TODO: font.styleHint
    readonly property int typefaceSans: 1
    readonly property int typefaceSerif: 2
    readonly property int typefaceMonospace: 3

    color: AndroidStyle.colorValue(styleDef.TextAppearance_textColor[key]) ||
           AndroidStyle.colorValue(styleDef.defaultTextColorPrimary)
    linkColor: AndroidStyle.colorValue(styleDef.TextAppearance_textColorLink[key])
    readonly property color hintColor: AndroidStyle.colorValue(styleDef.TextAppearance_textColorHint[key])
    readonly property color selectionColor: AndroidStyle.colorValue(styleDef.TextAppearance_textColorHighlight)
    readonly property color selectedTextColor: AndroidStyle.colorValue(styleDef.TextAppearance_textColor[selection])

    font.pixelSize: styleDef.TextAppearance_textSize
    font.bold: styleDef.TextAppearance_textStyle & textStyleBold
    font.italic: styleDef.TextAppearance_textStyle & textStyleItalic
    font.capitalization: styleDef.TextAppearance_textAllCaps ? Font.AllUppercase : Font.MixedCase

    maximumLineCount: styleDef.TextView_maxLines || Number.MAX_VALUE

    readonly property int ellipsize: styleDef.TextView_ellipsize || 0
    elide: ellipsize === ellipsizeMarquee ? Text.ElideRight : // TODO: marquee
           ellipsize === ellipsizeMiddle ? Text.ElideMiddle :
           ellipsize === ellipsizeStart ? Text.ElideLeft : Text.ElideRight

    readonly property int horizontalGravity: (styleDef.TextView_gravity || 0) & AndroidStyleBase.HORIZONTAL_GRAVITY_MASK
    horizontalAlignment: horizontalGravity == AndroidStyleBase.LEFT ? Qt.AlignLeft :
                         horizontalGravity == AndroidStyleBase.RIGHT ? Qt.AlignRight : Qt.AlignHCenter

    readonly property int verticalGravity: (styleDef.TextView_gravity || 0) & AndroidStyleBase.VERTICAL_GRAVITY_MASK
    verticalAlignment: verticalGravity == AndroidStyleBase.TOP ? Qt.AlignTop :
                       verticalGravity == AndroidStyleBase.BOTTOM ? Qt.AlignBottom : Qt.AlignVCenter
}
