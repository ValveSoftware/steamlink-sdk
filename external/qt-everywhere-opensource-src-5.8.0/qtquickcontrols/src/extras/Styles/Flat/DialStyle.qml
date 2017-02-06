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
import QtQuick.Controls.Styles 1.4 as Base
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Extras 1.4
import QtQuick.Extras.Private 1.0
import QtQuick.Extras.Private.CppUtils 1.1

Base.DialStyle {
    id: dialStyle

    handleInset: handleSize * 1.5 + Math.min(control.width, control.height) * 0.15

    readonly property int handleSize: Math.max(6, Math.round(Math.min(control.width, control.height) * 0.05))

    background: Item {
        implicitWidth: Math.round(160 * FlatStyle.scaleFactor)
        implicitHeight: Math.round(160 * FlatStyle.scaleFactor)

        CircularProgressBar {
            id: progressBar
            anchors.fill: parent
            progress: control.value
            minimumValueAngle: -150
            maximumValueAngle: 150
            barWidth: handleSize / 2
            backgroundColor: FlatStyle.grooveColor

            function updateColor() {
                clearStops();
                addStop(0, control.enabled ? (control.activeFocus ? FlatStyle.focusedColor : FlatStyle.styleColor) : FlatStyle.mediumFrameColor);
                redraw();
            }

            Component.onCompleted: updateColor()

            Connections {
                target: control
                onEnabledChanged: progressBar.updateColor()
                onActiveFocusChanged: progressBar.updateColor()
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: parent.width * 0.15
            radius: width / 2
            color: "white"
            border.width: control.enabled && control.activeFocus ? FlatStyle.twoPixels : FlatStyle.onePixel
            border.color: control.enabled ? (control.activeFocus ? FlatStyle.focusedColor : FlatStyle.styleColor) : FlatStyle.mediumFrameColor
        }
    }

    handle: Rectangle {
        color: control.enabled ? (control.activeFocus ? FlatStyle.focusedColor : FlatStyle.styleColor) : FlatStyle.mediumFrameColor
        width: handleSize
        height: width
        radius: height / 2
    }

    tickmark: null
}
