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
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

DrawableLoader {
    id: delegate

    property bool hasText: !!editor.text || !!editor.displayText

    styleDef: styleData.hasSelection ? AndroidStyle.styleDef.textViewStyle.TextView_textSelectHandleRight
                                     : AndroidStyle.styleDef.textViewStyle.TextView_textSelectHandle
    x: styleData.hasSelection ? -width / 4 : -width / 2
    y: styleData.lineHeight

    pressed: styleData.pressed
    focused: control.activeFocus
    window_focused: focused && control.Window.active

    opacity: hasText && (styleData.hasSelection || styleData.pressed || idle.running) ? 1.0 : 0.0

    Timer {
        id: idle
        repeat: false
        interval: 4000
    }

    Connections {
        target: styleData
        onActivated: idle.restart()
        onPressedChanged: {
            if (!styleData.pressed)
                idle.restart()
        }
    }

    // Hide the cursor handle on textual changes, unless the signals are
    // indirectly triggered by activating/tapping/moving the handle. When
    // text prediction is enabled, the textual content may change when the
    // cursor moves and the predicted text is committed.
    Connections {
        target: editor
        ignoreUnknownSignals: true
        onTextChanged: if (!ignore.running) idle.stop()
        onDisplayTextChanged: if (!ignore.running) idle.stop()
        onInputMethodComposing: if (!ignore.running) idle.stop()
    }

    Timer {
        id: ignore
        repeat: false
        interval: 250
        running: idle.running
    }
}
