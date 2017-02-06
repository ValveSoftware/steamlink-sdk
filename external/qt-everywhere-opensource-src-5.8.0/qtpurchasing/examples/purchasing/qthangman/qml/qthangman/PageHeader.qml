/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3-COMM$
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
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2

Item {
    id: header

    property alias title: titleText.text
    property int buttonHeight: topLevel.buttonHeight
    signal clicked()


    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    height: buttonHeight + (topLevel.globalMargin * 2)
    SimpleButton {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: topLevel.globalMargin
        height: buttonHeight
        width: parent.width * 0.25
        text: "Back"
        onClicked: {
            pageStack.pop();
            header.clicked();
        }
    }
    Text {
        id: titleText
        anchors.left: backButton.right
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: topLevel.globalMargin
        font.family: ".Helvetica Neue Interface -M3"
        color: "white"
        font.pointSize: 64
        fontSizeMode: Text.Fit
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        minimumPointSize: 8
    }
}
