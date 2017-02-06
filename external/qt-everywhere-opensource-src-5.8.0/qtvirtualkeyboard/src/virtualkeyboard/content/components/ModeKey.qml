/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0

/*!
    \qmltype ModeKey
    \inqmlmodule QtQuick.VirtualKeyboard
    \ingroup qtvirtualkeyboard-qml
    \inherits Key
    \since QtQuick.VirtualKeyboard 2.0

    \brief Generic mode key for keyboard layouts.

    This key provides generic mode button functionality.

    A key press toggles the current mode without emitting key event
    for input method processing.

    ModeKey can be used in situations where a particular mode is switched
    "ON / OFF", and where the mode change does not require changing the
    keyboard layout. When this component is used, the \l { BaseKey::displayText } { displayText } should
    remain the same regardless of the mode, because the keyboard style
    visualizes the status.
*/

Key {
    /*! This property provides the current mode.

        The default is false.
    */
    property bool mode
    noKeyEvent: true
    functionKey: true
    onClicked: mode = !mode
    keyPanelDelegate: keyboard.style ? keyboard.style.modeKeyPanel : undefined
}
