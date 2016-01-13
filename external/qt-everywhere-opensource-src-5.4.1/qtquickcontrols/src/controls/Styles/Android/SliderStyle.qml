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
import QtQuick.Window 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

Style {
    readonly property Slider control: __control

    property Component panel: Item {
        id: panel

        readonly property bool horizontal: control.orientation === Qt.Horizontal
        readonly property var styleDef: AndroidStyle.styleDef.seekBarStyle

        readonly property real thumbOffset: styleDef.thumbOffset || thumb.implicitWidth / 2

        readonly property real minWidth: styleDef.ProgressBar_minWidth || styleDef.View_minWidth || 0
        readonly property real minHeight: styleDef.ProgressBar_minHeight || styleDef.View_minHeight || 0

        readonly property real preferredWidth: Math.max(minWidth, track.implicitWidth)
        readonly property real preferredHeight: Math.max(minHeight, Math.max(thumb.implicitHeight, track.implicitHeight))

        implicitWidth: horizontal ? preferredWidth : preferredHeight
        implicitHeight: horizontal ? preferredHeight : preferredWidth

        y: horizontal ? 0 : height
        rotation: horizontal ? 0 : -90
        transformOrigin: Item.TopLeft

        Item {
            anchors.fill: parent
            DrawableLoader {
                id: track
                styleDef: panel.styleDef.ProgressBar_progressDrawable
                level: (control.value - control.minimumValue) / (control.maximumValue - control.minimumValue)
                excludes: [panel.styleDef.ProgressBar_secondaryProgress_id]
                clippables: [panel.styleDef.ProgressBar_progress_id]
                x: thumbOffset
                y: Math.round((Math.round(horizontal ? parent.height : parent.width) - track.height) / 2)
                width: (horizontal ? parent.width : parent.height) - 2 * thumbOffset
            }
            DrawableLoader {
                id: thumb
                pressed: control.pressed
                focused: control.activeFocus
                window_focused: control.Window.active
                styleDef: panel.styleDef.SeekBar_thumb
                x: Math.round((control.__handlePos - control.minimumValue) / (control.maximumValue - control.minimumValue) * ((horizontal ? panel.width : panel.height) - thumb.width))
            }
        }
    }
}
