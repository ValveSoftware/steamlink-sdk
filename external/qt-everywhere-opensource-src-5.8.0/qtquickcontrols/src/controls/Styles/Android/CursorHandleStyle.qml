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
    visible: opacity > 0.0 && focused

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
