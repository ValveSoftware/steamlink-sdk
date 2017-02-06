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
    \qmltype KeyboardLayoutLoader
    \inqmlmodule QtQuick.VirtualKeyboard
    \ingroup qtvirtualkeyboard-qml
    \inherits Loader
    \since QtQuick.VirtualKeyboard 1.1

    \brief Allows dynamic loading of keyboard layout.

    This type is useful for keyboard layouts consisting of multiple pages of keys.

    A single keyboard layout (a page) is defined by using the Component
    as a container. The active keyboard layout can then be changed by
    setting the sourceComponent property to a different value.

    Example:

    \code
    import QtQuick 2.0
    import QtQuick.Layouts 1.0
    import QtQuick.VirtualKeyboard 2.1

    // file: layouts/en_GB/symbols.qml

    KeyboardLayoutLoader {
        property bool secondPage
        onVisibleChanged: if (!visible) secondPage = false
        sourceComponent: secondPage ? page2 : page1
        Component {
            id: page1
            KeyboardLayout {
                // Keyboard layout definition for page 1
            }
        }
        Component {
            id: page2
            KeyboardLayout {
                // Keyboard layout definition for page 2
            }
        }
    }
    \endcode
*/

Loader {
    /*! Sets the input method for all the keyboard layouts loaded
        in this context.

        The input method can either be set separately for each keyboard
        layout, or commonly at this context. If set separately, then this
        property should not be modified.
    */
    property var inputMethod: item ? item.inputMethod : null

    /*! This function may be overridden by the keyboard layout
        to create the input method object dynamically. The default
        implementation forwards the call to the child keyboard
        layout.

        The input method object created by this function can outlive
        keyboard layout transitions in certain cases. In particular,
        this applies to the transitions between the layouts listed in
        the sharedLayouts property.
    */
    function createInputMethod() {
        return item ? item.createInputMethod() : null
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
    property var sharedLayouts: item ? item.sharedLayouts : null

    /*! Sets the input mode for all the keyboard layouts loaded
        in this context.

        The input mode can either be set separately for each keyboard
        layout, or commonly at this context. If set separately, then this
        property should not be modified.
    */
    property int inputMode: item ? item.inputMode : -1

    property int __updateCount

    active: parent !== null

    onItemChanged: if (parent && item && __updateCount++ > 0 && !keyboard.inputMethodNeedsReset) keyboard.updateInputMethod()
}
