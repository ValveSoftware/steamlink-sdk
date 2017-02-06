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
    \qmltype KeyPanel
    \inqmlmodule QtQuick.VirtualKeyboard.Styles
    \brief A base type of the styled keys.
    \ingroup qtvirtualkeyboard-styles-qml

    All the key delegates provided by the style should be based on this type.
*/

Item {
    /*! Provides access to properties in BaseKey.

        A list of available properties in control:
        \list
            \li \c control.key Unicode code of the key.
            \li \c control.text Unicode text of the key.
            \li \c control.displayText Display text of the key.
            \li \c control.smallText Small text of the key, usually rendered in the corner of the key.
            \li \c control.smallTextVisible Visibility of the small text.
            \li \c control.alternativeKeys List of alternative key sequences.
            \li \c control.enabled Set to true when the key is enabled.
            \li \c control.pressed Set to true when the key is currently pressed.
            \li \c control.uppercased Set to true when the key is uppercased.
            \li \c control.capsLock Set to true when caps lock is enabled.
            \note Deprecated since 1.2. Use \l {InputContext::capsLock} {InputContext.capsLock} instead.
        \endlist
    */
    property Item control

    /*!
        \since QtQuick.VirtualKeyboard.Styles 1.1

        Sets the sound effect to be played on key press.
    */
    property url soundEffect

    // Uncomment the following to reveal the key sizes
    /*
    Rectangle {
        id: root
        z: 1
        color: "transparent"
        border.color: "white"
        anchors.fill: parent
        Rectangle {
            color: "black"
            opacity: 0.6
            anchors.top: parent.top
            anchors.topMargin: 1
            anchors.left: parent.left
            anchors.leftMargin: 1
            implicitWidth: keyPanelInfoText.width + 4
            implicitHeight: keyPanelInfoText.height + 4
            Text {
                id: keyPanelInfoText
                text: root.parent.width + "x" + root.parent.height
                font.pixelSize: 12
                color: "white"
                anchors.centerIn: parent
            }
        }
    }
    */
}
