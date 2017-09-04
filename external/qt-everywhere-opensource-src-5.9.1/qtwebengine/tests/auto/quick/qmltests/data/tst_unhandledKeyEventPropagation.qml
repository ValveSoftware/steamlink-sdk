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

import QtQuick 2.0
import QtTest 1.0
import QtWebEngine 1.2

Item {
    id: parentItem
    width: 400
    height: 300

    property var pressEvents: []
    property var releaseEvents: []
    Keys.onPressed: pressEvents.push(event.key)
    Keys.onReleased: releaseEvents.push(event.key)

    TestWebEngineView {
        id: webEngineView
        anchors.fill: parent
        focus: true
    }
    TestCase {
        name: "WebEngineViewUnhandledKeyEventPropagation"

        when: false
        Timer {
            running: parent.windowShown
            repeat: false
            interval: 1
            onTriggered: parent.when = true
        }

        function test_keyboardModifierMapping() {
            webEngineView.url = Qt.resolvedUrl("keyboardEvents.html");
            verify(webEngineView.waitForLoadSucceeded());

            webEngineView.runJavaScript("document.getElementById('first_div').focus()");
            webEngineView.verifyElementHasFocus("first_div");

            keyPress(Qt.Key_Right);
            keyRelease(Qt.Key_Right);
            // Right arrow key is unhandled thus focus is not changed
            tryCompare(parentItem.releaseEvents, "length", 1);
            webEngineView.verifyElementHasFocus("first_div");

            keyPress(Qt.Key_Tab);
            keyRelease(Qt.Key_Tab);
            // Tab key is handled thus focus is changed
            tryCompare(parentItem.releaseEvents, "length", 2);
            webEngineView.verifyElementHasFocus("second_div");

            keyPress(Qt.Key_Left);
            keyRelease(Qt.Key_Left);
            // Left arrow key is unhandled thus focus is not changed
            tryCompare(parentItem.releaseEvents, "length", 3);
            webEngineView.verifyElementHasFocus("second_div");

            // The page will consume the Tab key to change focus between elements while the arrow
            // keys won't be used.
            compare(parentItem.pressEvents.length, 2);
            compare(parentItem.pressEvents[0], Qt.Key_Right);
            compare(parentItem.pressEvents[1], Qt.Key_Left);

            // Key releases will all come back unconsumed.
            compare(parentItem.releaseEvents[0], Qt.Key_Right);
            compare(parentItem.releaseEvents[1], Qt.Key_Tab);
            compare(parentItem.releaseEvents[2], Qt.Key_Left);
        }
    }
}
