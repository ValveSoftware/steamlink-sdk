/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Extras 1.4

ListView {
    id: stylePicker
    width: parent.width
    height: root.height * 0.06
    interactive: false
    spacing: -1

    orientation: ListView.Horizontal

    readonly property string currentStylePath: stylePicker.model.get(stylePicker.currentIndex).path
    readonly property bool currentStyleDark: stylePicker.model.get(stylePicker.currentIndex).dark !== undefined
        ? stylePicker.model.get(stylePicker.currentIndex).dark
        : true

    ExclusiveGroup {
        id: styleExclusiveGroup
    }

    delegate: Button {
        width: Math.round(stylePicker.width / stylePicker.model.count)
        height: stylePicker.height
        checkable: true
        checked: index === ListView.view.currentIndex
        exclusiveGroup: styleExclusiveGroup

        onCheckedChanged: {
            if (checked) {
                ListView.view.currentIndex = index;
            }
        }

        style: ButtonStyle {
            background: Rectangle {
                readonly property color checkedColor: currentStyleDark ? "#444" : "#777"
                readonly property color uncheckedColor: currentStyleDark ? "#222" : "#bbb"
                color: checked ? checkedColor : uncheckedColor
                border.color: checkedColor
                border.width: 1
                radius: 1
            }

            label: Text {
                text: name
                color: currentStyleDark ? "white" : (checked ? "white" : "black")
                font.pixelSize: root.toPixels(0.04)
                font.family: openSans.name
                anchors.centerIn: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
