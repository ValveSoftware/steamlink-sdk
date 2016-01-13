/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtWebEngine 1.0
import QtWebEngine.experimental 1.0

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
            var findFlags = WebEngineViewExperimental.FindCaseSensitively
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.experimental.findText("Hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }

        function test_findTextCaseInsensitive() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.experimental.findText("heLLo", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }

        function test_findTextManyMatches() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test4.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.experimental.findText("bla", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 100)
            verify(!findFailed)
        }


        function test_findTextFailCaseSensitive() {
            var findFlags = WebEngineViewExperimental.FindCaseSensitively
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.experimental.findText("heLLo", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)
        }

        function test_findTextNotFound() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.experimental.findText("string-that-is-not-threre", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)
        }
    }
}
