/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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
