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
import QtQuick.Controls 1.1
import org.qtproject.qthangman 1.0
import QtPurchasing 1.0

Item {
    id: topLevel

    height: 480
    width: 320
    property real buttonHeight: 0
    property real globalMargin: width * 0.025

    Component.onCompleted: forceActiveFocus()
    Keys.onBackPressed: {
        if (pageStack.depth === 1) {
            Qt.quit()
        } else {
            pageStack.pop()
            event.accepted = true
            forceActiveFocus()
        }
    }
    focus: true

    HangmanGame {
        id: applicationData
        onWordChanged: {
            if (word.length > 0)
                splashLoader.source = "";
        }
    }

    StackView {
        id: pageStack
        anchors.fill: topLevel
        initialItem: Qt.resolvedUrl("GameView.qml")
    }
    // ![0]
    Store {
        id: iapStore
    }
    // ![0]
}
