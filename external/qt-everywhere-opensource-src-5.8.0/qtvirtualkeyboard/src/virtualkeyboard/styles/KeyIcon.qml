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
    \qmltype KeyIcon
    \inqmlmodule QtQuick.VirtualKeyboard.Styles
    \brief Key icon with adjustable color.
    \ingroup qtvirtualkeyboard-styles-qml

    The KeyIcon item displays an icon with adjustable color.
*/

Item {
    /*! The icon color. */
    property alias color: overlay.color
    /*! The source image. */
    property alias source: icon.source
    Image {
        id: icon
        sourceSize.height: parent.height
        sourceSize.width: parent.width
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        visible: false
    }
    ShaderEffect {
        id: overlay
        property color color
        property variant texture: icon
        anchors.fill: icon
        fragmentShader: "
            uniform lowp vec4 color;
            uniform lowp float qt_Opacity;
            uniform lowp sampler2D texture;
            varying highp vec2 qt_TexCoord0;
            void main() {
                highp vec4 sample = texture2D(texture, qt_TexCoord0) * qt_Opacity;
                gl_FragColor = vec4(color.rgb, 1.0) * sample.a;
            }
            "
    }
}
