/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0

Image {
    id: is
    source: "images/bug.jpg"
    smooth: true
    anchors.fill: parent
    fillMode: Image.PreserveAspectFit
    sourceSize: width > height ? Qt.size(0, parent.height) : Qt.size(parent.width, 0)
    //visible: !enabledCheckBox.selected
    opacity: enabledCheckBox.selected ? 0.0 : 1.0

    property bool forcedUpdateAnimationRunning: updateCheckBox.selected
    Text {
        x: is.width - 10 - width
        y: 10
        text: "Qt"
        font.pixelSize: 20
        color: "white"
        visible: is.forcedUpdateAnimationRunning

        NumberAnimation on rotation {
            id: dd
            running: is.forcedUpdateAnimationRunning
            loops: Animation.Infinite
            duration: 5000
            from: 0.0
            to: 360.0
        }
    }
}
