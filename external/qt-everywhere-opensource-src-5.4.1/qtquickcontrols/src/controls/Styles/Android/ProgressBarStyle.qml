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
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

Style {
    readonly property ProgressBar control: __control

    property Component panel: Item {
        id: panel

        readonly property bool horizontal: control.orientation === Qt.Horizontal
        readonly property var styleDef: AndroidStyle.styleDef.progressBarStyleHorizontal

        readonly property real minWidth: styleDef.ProgressBar_minWidth || 0
        readonly property real minHeight: styleDef.ProgressBar_minHeight || 0
        readonly property real maxWidth: styleDef.ProgressBar_maxWidth || minWidth
        readonly property real maxHeight: styleDef.ProgressBar_maxHeight || minHeight

        readonly property real preferredWidth: Math.min(maxWidth, Math.max(minWidth, bg.implicitWidth))
        readonly property real preferredHeight: Math.min(maxHeight, Math.max(minHeight, bg.implicitHeight))

        implicitWidth: horizontal ? preferredWidth : preferredHeight
        implicitHeight: horizontal ? preferredHeight : preferredWidth

        DrawableLoader {
            id: bg
            width: horizontal ? parent.width : parent.height
            height: !horizontal ? parent.width : parent.height
            y: horizontal ? 0 : width
            rotation: horizontal ? 0 : -90
            transformOrigin: Item.TopLeft
            styleDef: control.indeterminate ? panel.styleDef.ProgressBar_indeterminateDrawable
                                            : panel.styleDef.ProgressBar_progressDrawable
            level: (control.value - control.minimumValue) / (control.maximumValue - control.minimumValue)
            levelId: panel.styleDef.ProgressBar_progress_id
            excludes: [panel.styleDef.ProgressBar_secondaryProgress_id]
        }
    }
}
