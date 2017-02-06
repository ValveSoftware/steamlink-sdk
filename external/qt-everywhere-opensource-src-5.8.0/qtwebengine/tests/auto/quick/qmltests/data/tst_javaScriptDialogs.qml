/****************************************************************************
**
** Copyright (C) 2015 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
import QtWebEngine.testsupport 1.0
import "../mock-delegates/TestParams" 1.0

TestWebEngineView {
    id: webEngineView

    testSupport: WebEngineTestSupport {
        property bool windowCloseRejectedSignalEmitted: false

        function waitForWindowCloseRejected() {
            return _waitFor(function () {
                    return testSupport.windowCloseRejectedSignalEmitted;
                });
        }

        onWindowCloseRejected: {
            windowCloseRejectedSignalEmitted = true;
        }
    }

    TestCase {
        id: test
        name: "WebEngineViewJavaScriptDialogs"

        function init() {
            JSDialogParams.dialogMessage = "";
            JSDialogParams.dialogTitle = "";
            JSDialogParams.dialogCount = 0;
            JSDialogParams.shouldAcceptDialog = true;
        }

        function test_alert() {
            webEngineView.url = Qt.resolvedUrl("alert.html")
            verify(webEngineView.waitForLoadSucceeded())
            compare(JSDialogParams.dialogCount, 1)
            compare(JSDialogParams.dialogMessage, "Hello Qt")
            verify(JSDialogParams.dialogTitle.indexOf("Javascript Alert -") === 0)
        }

        function test_confirm() {
            webEngineView.url = Qt.resolvedUrl("confirm.html")
            verify(webEngineView.waitForLoadSucceeded())
            compare(JSDialogParams.dialogMessage, "Confirm test")
            compare(JSDialogParams.dialogCount, 1)
            compare(webEngineView.title, "ACCEPTED")
            JSDialogParams.shouldAcceptDialog = false
            webEngineView.reload()
            verify(webEngineView.waitForLoadSucceeded())
            compare(JSDialogParams.dialogCount, 2)
            compare(webEngineView.title, "REJECTED")

        }

        function test_confirmClose() {
            webEngineView.url = Qt.resolvedUrl("confirmclose.html");
            verify(webEngineView.waitForLoadSucceeded());
            webEngineView.windowCloseRequestedSignalEmitted = false;
            JSDialogParams.shouldAcceptDialog = true;
            webEngineView.triggerWebAction(WebEngineView.RequestClose);
            verify(webEngineView.waitForWindowCloseRequested());
        }

        function test_rejectClose() {
            webEngineView.url = Qt.resolvedUrl("confirmclose.html");
            verify(webEngineView.waitForLoadSucceeded());
            webEngineView.testSupport.windowCloseRejectedSignalEmitted = false;
            JSDialogParams.shouldAcceptDialog = false;
            webEngineView.triggerWebAction(WebEngineView.RequestClose);
            verify(webEngineView.testSupport.waitForWindowCloseRejected());
        }

        function test_prompt() {
            JSDialogParams.inputForPrompt = "tQ olleH"
            webEngineView.url = Qt.resolvedUrl("prompt.html")
            verify(webEngineView.waitForLoadSucceeded())
            compare(JSDialogParams.dialogCount, 1)
            compare(webEngineView.title, "tQ olleH")
            JSDialogParams.shouldAcceptDialog = false
            webEngineView.reload()
            verify(webEngineView.waitForLoadSucceeded())
            compare(JSDialogParams.dialogCount, 2)
            compare(webEngineView.title, "null")
        }
    }
}
