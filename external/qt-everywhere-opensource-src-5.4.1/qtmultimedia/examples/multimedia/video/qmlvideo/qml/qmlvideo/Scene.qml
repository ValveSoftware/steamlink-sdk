/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0

Rectangle {
    id: root
    color: "black"
    property alias buttonHeight: closeButton.height
    property string source1
    property string source2
    property int contentWidth: parent.width / 2
    property real volume: 0.25
    property int margins: 5
    property QtObject content

    signal close
    signal videoFramePainted

    Button {
        id: closeButton
        anchors {
            top: parent.top
            right: parent.right
            margins: root.margins
        }
        width: Math.max(parent.width, parent.height) / 12
        height: Math.min(parent.width, parent.height) / 12
        z: 2.0
        bgColor: "#212121"
        bgColorSelected: "#757575"
        textColorSelected: "white"
        text: "Back"
        onClicked: root.close()
    }
}
