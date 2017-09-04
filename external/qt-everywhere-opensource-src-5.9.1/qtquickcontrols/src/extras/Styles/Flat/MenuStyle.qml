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
import QtQuick 2.2
import QtQuick.Controls.Styles 1.2 as Base
import QtQuick.Controls.Styles.Flat 1.0

Base.MenuStyle {
    id: style
    font.family: FlatStyle.fontFamily
    __leftLabelMargin: Math.round(30 * FlatStyle.scaleFactor)

    frame: Rectangle {
        border.color: FlatStyle.darkFrameColor
        border.width: FlatStyle.onePixel
    }

    itemDelegate.background: Rectangle {
        color: !!styleData.pressed || styleData.selected ? FlatStyle.disabledColor : "transparent"
        opacity: !!styleData.pressed || styleData.selected ? 0.15 : 1.0
    }

    itemDelegate.label: Text {
        text: formatMnemonic(styleData.text, styleData.underlineMnemonic)
        renderType: FlatStyle.__renderType
        color: FlatStyle.defaultTextColor
        font.family: FlatStyle.fontFamily
        font.pixelSize: FlatStyle.defaultFontSize
        verticalAlignment: Text.AlignVCenter
        height: Math.round(26 * FlatStyle.scaleFactor)
    }

    itemDelegate.shortcut: Text {
        text: styleData.shortcut
        renderType: FlatStyle.__renderType
        color: FlatStyle.defaultTextColor
        font.family: FlatStyle.fontFamily
        font.pixelSize: FlatStyle.defaultFontSize
    }

    itemDelegate.checkmarkIndicator: CheckBoxDrawer {
        visible: styleData.checked
        controlEnabled: styleData.enabled
        controlChecked: styleData.checked
        backgroundVisible: false
        x: -4 // ### FIXME: compensate hardcoded "x: 4" in MenuStyle
        y: FlatStyle.onePixel
    }

    itemDelegate.submenuIndicator: LeftArrowIcon {
        scale: -1
        color: "#000000"
        width: Math.round(10 * FlatStyle.scaleFactor)
        height: Math.round(10 * FlatStyle.scaleFactor)
        baselineOffset: Math.round(7 * FlatStyle.scaleFactor)
    }

    separator: Rectangle {
        color: FlatStyle.lightFrameColor
        width: parent.width
        implicitHeight: FlatStyle.onePixel
    }
}
