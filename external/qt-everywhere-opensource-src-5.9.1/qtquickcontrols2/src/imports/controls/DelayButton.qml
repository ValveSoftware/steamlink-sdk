/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.impl 2.2
import QtQuick.Templates 2.2 as T

T.DelayButton {
    id: control

    implicitWidth: Math.max(background ? background.implicitWidth : 0,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(background ? background.implicitHeight : 0,
                             contentItem.implicitHeight + topPadding + bottomPadding)
    baselineOffset: contentItem.y + contentItem.baselineOffset

    padding: 6
    leftPadding: padding + 2
    rightPadding: padding + 2

    transition: Transition {
        NumberAnimation {
            duration: control.delay * (control.pressed ? 1.0 - control.progress : 0.3 * control.progress)
        }
    }

    contentItem: Item {
        implicitWidth: label.implicitWidth
        implicitHeight: label.implicitHeight

        Item {
            x: -control.leftPadding + (control.progress * control.width)
            width: (1.0 - control.progress) * control.width
            height: parent.height

            clip: control.progress > 0
            visible: control.progress < 1

            Text {
                id: label
                x: -parent.x
                width: control.availableWidth
                height: parent.height

                text: control.text
                font: control.font
                opacity: enabled ? 1 : 0.3
                color: control.visualFocus ? Default.focusColor : (control.down ? Default.textDarkColor : Default.textColor)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        Item {
            x: -control.leftPadding
            width: control.progress * control.width
            height: parent.height

            clip: control.progress > 0
            visible: control.progress > 0

            Text {
                x: control.leftPadding
                width: control.availableWidth
                height: parent.height

                text: control.text
                font: control.font
                opacity: enabled ? 1 : 0.3
                color: Default.textLightColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 40
        color: control.visualFocus ? (control.down ? Default.focusPressedColor : Default.focusLightColor) : (control.down ? Default.buttonPressedColor : Default.buttonColor)
        border.color: Default.focusColor
        border.width: control.visualFocus ? 2 : 0

        Rectangle {
            width: control.progress * parent.width
            height: parent.height
            color: control.visualFocus ? (control.down ? Default.buttonCheckedFocusColor : Default.focusColor) : (control.down ? Default.buttonCheckedPressedColor : Default.textColor)
        }
    }
}
