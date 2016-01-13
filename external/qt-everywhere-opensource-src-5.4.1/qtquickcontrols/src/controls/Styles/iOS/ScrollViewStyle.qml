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
import QtQuick.Controls.Styles 1.3
import QtQuick.Controls.Private 1.0

ScrollViewStyle {
    corner: null
    incrementControl: null
    decrementControl: null
    frame: null
    scrollBarBackground: null
    padding { top: 0; left: 0; right: 0; bottom: 0 }

    __scrollBarFadeDelay: 50
    __scrollBarFadeDuration: 200
    __stickyScrollbars: true

    handle: Item {
        implicitWidth: 2.5
        implicitHeight: 2.5

        anchors.top: !styleData.horizontal ? parent.top : undefined
        anchors.left: styleData.horizontal ? parent.left : undefined
        anchors.bottom: parent.bottom
        anchors.right: parent.right

        anchors.leftMargin: implicitWidth
        anchors.topMargin: implicitHeight + 1
        anchors.rightMargin: implicitWidth * (!styleData.horizontal ? 2 : 4)
        anchors.bottomMargin: implicitHeight * (styleData.horizontal ? 2 : 4)

        Behavior on width { enabled: !styleData.horizontal; NumberAnimation { duration: 100 } }
        Behavior on height { enabled: styleData.horizontal; NumberAnimation { duration: 100 } }

        Loader {
            anchors.fill: parent
            sourceComponent: styleData.horizontal ? horzontalHandle : verticalHandle
        }

        Component {
            id: verticalHandle
            Rectangle {
                color: "black"
                opacity: 0.3
                radius: parent.parent.implicitWidth
                width: parent.width
                y: overshootBottom > 0 ? parent.height - height : 0
                height: Math.max(8, parent.height - overshootTop - overshootBottom)

                property real overshootTop: -Math.min(0, __control.flickableItem.contentY)
                property real overshootBottom: Math.max(0, __control.flickableItem.contentY
                                                        - __control.flickableItem.contentHeight
                                                        + __control.flickableItem.height)
            }
        }

        Component {
            id: horzontalHandle
            Rectangle {
                color: "black"
                opacity: 0.3
                radius: parent.parent.implicitHeight
                height: parent.height
                x: overshootRight > 0 ? parent.width - width : 0
                width: Math.max(8, parent.width - overshootLeft - overshootRight)

                property real overshootLeft: -Math.min(0, __control.flickableItem.contentX)
                property real overshootRight: Math.max(0, __control.flickableItem.contentX
                                                       - __control.flickableItem.contentWidth
                                                       + __control.flickableItem.width)
            }
        }
    }
}
