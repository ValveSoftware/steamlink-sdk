/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.8
import QtQuick.Controls 2.1
import QtQuick.Controls.impl 2.1

Rectangle {
    implicitWidth: 28
    implicitHeight: 28

    radius: width / 2
    color: control.down
        ? (control.visualFocus ? Default.focusPressedColor : Default.indicatorPressedColor)
        : (control.visualFocus ? Default.focusLightColor : Default.backgroundColor)
    border.width: control.visualFocus ? 2 : 1
    border.color: control.visualFocus ? Default.focusColor : (control.down ? Default.indicatorFramePressedColor : Default.indicatorFrameColor)
    opacity: enabled ? 1 : 0.3

    property Item control

    Rectangle {
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 20
        height: 20
        radius: width / 2
        color: control.down ? Default.textDarkColor : Default.buttonCheckedColor
        visible: control.checked
    }
}
