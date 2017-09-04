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
import QtQuick.Window 2.2
import QtQuick.VirtualKeyboard 2.1

/*!
    \qmltype HandwritingInputPanel
    \inqmlmodule QtQuick.VirtualKeyboard
    \since QtQuick.VirtualKeyboard 2.0

    \brief Provides a handwriting panel add-on for the virtual keyboard UI.
    \ingroup qtvirtualkeyboard-qml

    The HandwritingInputPanel is an add-on component for the InputPanel, which
    enables full-screen handwriting input for the application.

    HandwritingInputPanel is designed to be anchored full screen alongside
    the InputPanel. The operating principle is that when the handwriting panel
    is "available", the InputPanel is invisible. This functionality is built-in,
    and requires no more than a reference to the InputPanel instance.

    The panel is set into operation by setting the \l {HandwritingInputPanel::}{available}
    property to \c true. When the panel is in operation, the keyboard remains hidden
    when the input focus is set. When \c available is \c true, handwriting input is
    activated by setting the \l {HandwritingInputPanel::}{active} property to \c true.

    The user interface, which provides controls for handwriting mode and the
    visibility of the keyboard, is application-specific. One suggested implementation
    is to use a floating button on the handwriting panel, where single click toggles
    the handwriting mode (changes the \l {HandwritingInputPanel::}{active} property), and double-click toggles
    the visibility of the keyboard (changes the \l {HandwritingInputPanel::}{available} property).

    HandwritingInputPanel also provides a word candidate popup which allows the user
    to select an alternative word candidate from the list of suggestions generated
    by the handwriting input method.
*/

Item {
    id: handwritingInputPanel

    /*! A reference to the input panel instance.

        This property must be set to the existing input panel instance.
    */
    property var inputPanel

    /*! This property controls the availability status of the handwriting input method.

        Setting the property to \c true prepares the handwriting input method and inhibits
        the display of keyboard.
    */
    property bool available

    /*! This property controls the active status of the handwriting input method.

        Setting the property to \c true activates the handwriting input method. When the
        handwriting input method is active, all touch input is captured by the
        handwriting input panel and redirected to input engine for processing.
    */
    property bool active

    state: enabled && available ? (active ? "active" : "available") : "unavailable"
    enabled: inputPanel.keyboard.isHandwritingAvailable()
    visible: enabled && available && active && Qt.inputMethod.visible

    Item {
        id: keyboard
        property var style: inputPanel && inputPanel.hasOwnProperty ? inputPanel.keyboard.style : null
        property var soundEffect: inputPanel && inputPanel.hasOwnProperty ? inputPanel.keyboard.soundEffect : null
    }

    onEnabledChanged: inputPanel.keyboard.fullScreenHandwritingMode = enabled && available
    onAvailableChanged: inputPanel.keyboard.fullScreenHandwritingMode = enabled && available

    TraceInputArea {
        id: hwrInputArea
        enabled: handwritingInputPanel.enabled && handwritingInputPanel.available && handwritingInputPanel.active
        objectName: "hwrInputArea"
        anchors.fill: parent
        patternRecognitionMode: InputEngine.HandwritingRecoginition
        canvasType: "fullscreen"
    }

    Binding {
        target: InputContext
        property: "keyboardRectangle"
        value: Qt.rect(hwrInputArea.x, hwrInputArea.y, hwrInputArea.width, hwrInputArea.height)
        when: handwritingInputPanel.enabled && handwritingInputPanel.available && handwritingInputPanel.active
    }

    Binding {
        target: inputPanel ? inputPanel.keyboard : null
        property: "active"
        value: false
        when: handwritingInputPanel.enabled && handwritingInputPanel.available
    }

    WordCandidatePopupList {
        z: 1
        objectName: "wordCandidatePopupList"
        enabled: handwritingInputPanel.enabled && handwritingInputPanel.available && handwritingInputPanel.active
    }
}
