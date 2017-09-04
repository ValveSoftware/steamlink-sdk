/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

import QtQuick 2.3
import QtTest 1.0
import QtWebEngine 1.2

Item {
width: 300
height: 400
    TextInput {
        id: textInput
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        focus: true
        text: Qt.resolvedUrl("test1.html")
        onEditingFinished: webEngineView.url = text
    }

    TestWebEngineView {
        id: webEngineView
        anchors {
            top: textInput.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        TestCase {
            name: "WebEngineViewLoadRecursionCrash"
            when:windowShown

            function test_QTBUG_42929() {
                textInput.forceActiveFocus()
                keyClick(Qt.Key_Return)
                verify(webEngineView.waitForLoadSucceeded())
                textInput.text = "about:blank"
                textInput.forceActiveFocus()
                keyClick(Qt.Key_Return)
                verify(webEngineView.waitForLoadSucceeded())
                textInput.text = Qt.resolvedUrl("test4.html")
                textInput.forceActiveFocus()
                // Don't crash now
                keyClick(Qt.Key_Return)
                verify(webEngineView.waitForLoadSucceeded())
            }
        }
    }
}
