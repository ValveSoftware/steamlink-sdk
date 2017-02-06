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

TestWebEngineView {
    id: webEngineView
    width: 400
    height: 300

    property int matchCount: 0
    property bool findFailed: false

    function clear() {
        findFailed = false
        matchCount = -1
    }

    function findTextCallback(matchCount) {
        webEngineView.matchCount = matchCount
        findFailed = matchCount == 0
    }


    TestCase {
        name: "WebViewFindText"

        function test_findText() {
            var findFlags = WebEngineView.FindCaseSensitively
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("Hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }

        function test_findTextCaseInsensitive() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("heLLo", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }

        function test_findTextManyMatches() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test4.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("bla", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 100)
            verify(!findFailed)
        }


        function test_findTextFailCaseSensitive() {
            var findFlags = WebEngineView.FindCaseSensitively
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("heLLo", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)
        }

        function test_findTextNotFound() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("string-that-is-not-threre", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)
        }

        function test_findTextAfterNotFound() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("about:blank")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)

            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }
    }
}
