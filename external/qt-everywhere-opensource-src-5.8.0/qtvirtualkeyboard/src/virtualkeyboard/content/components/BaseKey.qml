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
import QtQuick.Layouts 1.0

/*!
    \qmltype BaseKey
    \inqmlmodule QtQuick.VirtualKeyboard
    \ingroup qtvirtualkeyboard-qml
    \inherits Item

    \brief Common parent for all key types.

    BaseKey is a common type for all keys in keyboard layout.

    This type should not be used directly in the layouts. The specialized
    key types, such as Key or EnterKey should be used instead.
*/

Item {
    id: keyItem

    /*! Sets the key weight value which determines the relative size of the key.

        Use this property to change the key size in the layout.

        The default value is inherited from the parent element
        of the key in the layout hierarchy.
    */
    property real weight: parent.keyWeight

    /*! Sets the key text for input method processing.

        In most cases, this is the Unicode representation of the key code.

        The default value is an empty string.
    */
    property string text: ""

    /*! Sets the display text.

        This string is rendered in the keyboard layout.

        The default value is the key text.
    */
    property string displayText: text

    /*! \since QtQuick.VirtualKeyboard 2.0

        Sets the small text rendered in the corner of the key.

        The default value based on the default item in the effective alternative keys.
    */
    property string smallText: effectiveAlternativeKeysHighlightIndex !== -1 ? effectiveAlternativeKeys[effectiveAlternativeKeysHighlightIndex] : ""

    /*! \since QtQuick.VirtualKeyboard 2.0

        Sets the visibility of small text.

        The default value is inherited from the parent.
    */
    property bool smallTextVisible: parent.smallTextVisible

    /*! Sets the list of alternative keys.

        This property can be set to a string, or a list of strings. If the value is
        a string, the alternative keys are presented as individual characters of
        that string. If the value is a list of strings, the list is used instead.

        The alternative keys are presented to the user by pressing and holding a key
        with this property set.

        \note If the alternative keys contains the key \c text, it will be filtered from
              the \c effectiveAlternativeKeys and its position will be used as an indicator
              for the highlighted item instead.

        The default is empty list.
    */
    property var alternativeKeys: []

    /*! \since QtQuick.VirtualKeyboard 2.0

        This property contains the effective alternative keys presented to user.

        The list is contains the items in the \c alternativeKeys excluding the \c text
        item.
    */
    readonly property var effectiveAlternativeKeys: {
        var textIndex = alternativeKeys.indexOf(text)
        if (textIndex == -1)
            return alternativeKeys
        return alternativeKeys.slice(0, textIndex).concat(alternativeKeys.slice(textIndex + 1))
    }

    /*! \since QtQuick.VirtualKeyboard 2.0

        This property contains the index of highlighted item in the \c effectiveAlternativeKeys.

        The index is calculated from the index of the key \c text in the \c alternativeKeys.

        For example, if the alternative keys contains "çcċčć" and the key \c text is "c",
        this index will become 1 and the effective alternative keys presented to user will
        be "ç[ċ]čć".
    */
    readonly property int effectiveAlternativeKeysHighlightIndex: {
        var index = alternativeKeys.indexOf(text)
        return index > 1 && (index + 1) == alternativeKeys.length ? alternativeKeys.length - 2 : index
    }

    /*! Sets the key code for input method processing.

        The default is Qt.Key_unknown.
    */
    property int key: Qt.Key_unknown

    /*! \since QtQuick.VirtualKeyboard 1.3

        This property controls whether the key emits key events for input
        method processing. When true, the key events are disabled.

        By default, the key event is emitted if the \e key is not unknown
        or the \e text is not empty.
    */
    property bool noKeyEvent: key === Qt.Key_unknown && text.length === 0

    /*! This property holds the active status of the key.

        This property is automatically set to true when the key is pressed.
    */
    property bool active: false

    /*! \since QtQuick.VirtualKeyboard 1.3

        Disables key modifiers on the emitted key.

        The default is false.
    */
    property bool noModifier: false

    /*! Sets the key repeat attribute.

        If the repeat is enabled, the key will repeat the input events while held down.
        The repeat should not be used if alternativeKeys is also set.

        The default is false.
    */
    property bool repeat: false

    /*! Sets the highlighted status of the key.

        The default is false.
    */
    property bool highlighted: false

    /*! Sets the function key attribute.

        The default is false.
    */
    property bool functionKey: false

    /*! Sets the show preview attribute.

        By default, the character preview popup is not shown for function keys.
    */
    property bool showPreview: enabled && !functionKey && !keyboard.navigationModeActive

    /*! This property holds the pressed status of the key.

        The pressed status can only be true if the key is both enabled and active.
        When the key state becomes pressed, it triggers a key down event for the
        input engine. A key up event is triggered when the key is released.
    */
    property bool pressed: enabled && active

    /*! This property holds the uppercase status of the key.

        By default, this property reflects the uppercase status of the keyboard.
    */
    property bool uppercased: keyboard.uppercased && !noModifier

    /*! Sets the key panel delegate for the key.

        This property is essential for key decoration. Without a key panel delegate,
        the key is invisible. This property should be assigned in the inherited key type.
    */
    property alias keyPanelDelegate: keyPanel.sourceComponent

    /*!
        \since QtQuick.VirtualKeyboard 1.1

        This property holds the sound effect to be played on key press.

        This property is read-only since the sound effects are defined in the keyboard style.
    */
    readonly property url soundEffect: keyPanel.item ? keyPanel.item.soundEffect : ""

    onSoundEffectChanged: keyboard.soundEffect.register(soundEffect)

    // QTBUG-54953, QTBUG-55773
    // Avoid a row that was hidden taking up the entire height of the
    // keyboard when it is made visible after the application has started.
    // This value is low because keys can scale vertically, and setting e.g. 40
    // pixels might be too high for a keyboard that doesn't have a lot of space.
    implicitHeight: 1

    Layout.minimumWidth: keyPanel.implicitWidth
    Layout.minimumHeight: keyPanel.implicitHeight
    Layout.preferredWidth: weight
    Layout.fillWidth: true
    Layout.fillHeight: true

    Loader {
        id: keyPanel
        anchors.fill: parent
        onStatusChanged: if (status == Loader.Ready) keyPanel.item.control = keyItem
    }

    /*! This signal is triggered when the key is pressed, allowing custom processing
        of key.
    */
    signal clicked
}
