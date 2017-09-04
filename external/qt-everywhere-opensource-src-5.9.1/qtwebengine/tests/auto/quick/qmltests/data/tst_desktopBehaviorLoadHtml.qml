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
    width: 200
    height: 400
    focus: true

    property string lastUrl

    SignalSpy {
        id: linkHoveredSpy
        target: webEngineView
        signalName: "linkHovered"
    }

    onLinkHovered: {
        webEngineView.lastUrl = hoveredUrl
    }

    TestCase {
        name: "DesktopWebEngineViewLoadHtml"

        // Delayed windowShown to workaround problems with Qt5 in debug mode.
        when: false
        Timer {
            running: parent.windowShown
            repeat: false
            interval: 1
            onTriggered: parent.when = true
        }

        function init() {
            webEngineView.lastUrl = ""
            linkHoveredSpy.clear()
        }

        function test_baseUrlAfterLoadHtml() {
            linkHoveredSpy.clear()
            compare(linkHoveredSpy.count, 0)
            mouseMove(webEngineView, 150, 300)
            webEngineView.loadHtml("<html><head><title>Test page with huge link area</title></head><body><a title=\"A title\" href=\"test1.html\"><img width=200 height=200></a></body></html>", "http://www.example.foo.com")
            verify(webEngineView.waitForLoadSucceeded())

            // We get a linkHovered signal with empty hoveredUrl after page load
            linkHoveredSpy.wait()
            compare(linkHoveredSpy.count, 1)
            compare(webEngineView.lastUrl, "")

            compare(webEngineView.url, "http://www.example.foo.com/")
            mouseMove(webEngineView, 100, 100)
            linkHoveredSpy.wait()
            compare(linkHoveredSpy.count, 2)
            compare(webEngineView.lastUrl, "http://www.example.foo.com/test1.html")
        }
    }
}
