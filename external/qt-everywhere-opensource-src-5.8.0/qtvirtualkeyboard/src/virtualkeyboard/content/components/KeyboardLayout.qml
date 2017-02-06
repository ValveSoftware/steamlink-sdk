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
import QtQuick.VirtualKeyboard 2.1

/*!
    \qmltype KeyboardLayout
    \inqmlmodule QtQuick.VirtualKeyboard
    \ingroup qtvirtualkeyboard-qml
    \inherits ColumnLayout

    \brief Keyboard layout.

    This type is the root element of the keyboard layout.
    Use this element to build a new keyboard layout.

    Example:

    \code
    import QtQuick 2.0
    import QtQuick.Layouts 1.0
    import QtQuick.VirtualKeyboard 2.1

    // file: layouts/en_GB/main.qml

    KeyboardLayout {
        KeyboardRow {
            Key {
                key: Qt.Key_Q
                text: "q"
            }
            Key {
                key: Qt.Key_W
                text: "w"
            }
            Key {
                key: Qt.Key_E
                text: "e"
            }
            Key {
                key: Qt.Key_R
                text: "r"
            }
            Key {
                key: Qt.Key_T
                text: "t"
            }
            Key {
                key: Qt.Key_Y
                text: "y"
            }
        }
    }
    \endcode
*/

ColumnLayout {
    /*! Sets the input method to be used in this layout.

        This property allows a custom input method to be
        used in this layout.
    */
    property var inputMethod

    /*! This function may be overridden by the keyboard layout
        to create the input method object dynamically. The default
        implementation returns \c null.

        The input method object created by this function can outlive
        keyboard layout transitions in certain cases. In particular,
        this applies to the transitions between the layouts listed in
        the sharedLayouts property.
    */
    function createInputMethod() {
        return null
    }

    /*! List of layout names which share the input method created
        by the createInputMethod() function.

        If the list is empty (the default) the input method is not
        shared with any other layout and will be destroyed when the
        layout changes.

        The list should contain only the name of the layout type,
        e.g., ['symbols']. The current layout does not have to be
        included in the list.
    */
    property var sharedLayouts

    /*! Sets the input mode to be used in this layout.

        By default, the virtual keyboard attempts to preserve
        the current input mode when switching to a different
        keyboard layout.

        If the current input mode is not valid in the current
        context, the default input mode is specified by the
        input method.
    */
    property int inputMode: -1

    /*! Sets the key weight for all children keys.

        The default value is inherited from the parent element
        in the layout hierarchy.
    */
    property real keyWeight

    /*! \since QtQuick.VirtualKeyboard 2.0

        Sets the \c smallTextVisible for all children keys.

        The default value is inherited from the parent element
        in the layout hierarchy.
    */
    property bool smallTextVisible

    spacing: 0
}
