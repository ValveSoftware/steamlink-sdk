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
import QtQuick.Controls.Styles 1.2
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

ScrollViewStyle {
    handleOverlap: 0
    transientScrollBars: true
    minimumHandleLength: AndroidStyle.styleDef.scrollViewStyle.View_scrollbarSize

    __scrollBarFadeDelay: AndroidStyle.styleDef.scrollViewStyle.View_scrollbarDefaultDelayBeforeFade || 300
    __scrollBarFadeDuration: AndroidStyle.styleDef.scrollViewStyle.View_scrollbarFadeDuration || 250

    padding { top: 0; left: 0; right: 0; bottom: 0 }

    corner: null
    incrementControl: null
    decrementControl: null

    frame: DrawableLoader {
        active: control.frameVisible
        visible: !!control["backgroundVisible"]
        styleDef: AndroidStyle.styleDef.scrollViewStyle.View_background
    }

    scrollBarBackground: Item {
        implicitWidth: Math.max(minimumHandleLength, track.implicitWidth)
        implicitHeight: Math.max(minimumHandleLength, track.implicitHeight)
        DrawableLoader {
            id: track
            styleDef: styleData.horizontal ? AndroidStyle.styleDef.scrollViewStyle.View_scrollbarTrackHorizontal
                                           : AndroidStyle.styleDef.scrollViewStyle.View_scrollbarTrackVertical
            focused: control.activeFocus
            window_focused: control.Window.active
        }
    }

    handle: DrawableLoader {
        styleDef: styleData.horizontal ? AndroidStyle.styleDef.scrollViewStyle.View_scrollbarThumbHorizontal
                                       : AndroidStyle.styleDef.scrollViewStyle.View_scrollbarThumbVertical
        pressed: styleData.pressed
        focused: control.activeFocus
        window_focused: control.Window.active
    }
}
