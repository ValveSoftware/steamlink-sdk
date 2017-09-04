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

import QtQuick 2.2
import QtTest 1.0
import QtWebEngine 1.3

TestWebEngineView {
    id: webEngineView
    width: 300
    height: 400

    property var testUrl: Qt.resolvedUrl("test4.html")

    SignalSpy {
        id: scrollPositionSpy
        target: webEngineView
        signalName: "onScrollPositionChanged"
    }

    TestCase {
        name: "ScrollPosition"
        when: windowShown

        function init() {
            webEngineView.url = Qt.resolvedUrl("about:blank");
            verify(webEngineView.waitForLoadSucceeded());
        }

        function test_scrollPosition() {
            webEngineView.url = testUrl;
            verify(webEngineView.waitForLoadSucceeded());

            keyPress(Qt.Key_Return); // Focus is on the scroll button.

            tryCompare(scrollPositionSpy, "count", 1);
            compare(webEngineView.scrollPosition.x, 0);
            compare(webEngineView.scrollPosition.y, 600);
        }

        function test_scrollPositionAfterReload() {
            webEngineView.url = testUrl;
            verify(webEngineView.waitForLoadSucceeded());
            tryCompare(webEngineView.scrollPosition, "y", 0);

            keyPress(Qt.Key_Return); // Focus is on the scroll button.

            // Wait for proper scroll position change otherwise we cannot expect
            // the new y position after reload.
            tryCompare(webEngineView.scrollPosition, "x", 0);
            tryCompare(webEngineView.scrollPosition, "y", 600);

            webEngineView.reload();
            verify(webEngineView.waitForLoadSucceeded());

            tryCompare(webEngineView.scrollPosition, "x", 0);
            tryCompare(webEngineView.scrollPosition, "y", 600);
        }
    }
}
