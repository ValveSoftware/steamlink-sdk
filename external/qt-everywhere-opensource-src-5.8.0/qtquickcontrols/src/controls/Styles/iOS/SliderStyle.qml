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

import QtQuick 2.3
import QtQuick.Controls.Styles 1.3

SliderStyle {
    groove: Rectangle {
        implicitWidth: 20
        implicitHeight: 2

        color: "#a8a8a8"
        radius: 45.0

        Rectangle {
            width: styleData.handlePosition
            height: parent.height
            color: "#0a60ff"
            radius: parent.radius
        }
    }

    handle: Item {
        width: 29
        height: 32

        Rectangle {
            y: 3
            width: 29
            height: 29
            radius: 90.0

            color: "#d6d6d6"
            opacity: 0.2
        }

        Rectangle {
            width: 29
            height: 29
            radius: 90.0

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#e2e2e2" }
                GradientStop { position: 1.0; color: "#d6d6d6" }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: parent.radius
            }
        }
    }
}
