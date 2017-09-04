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

    SignalSpy {
        id: spy
        target: webEngineView
        signalName: "titleChanged"
    }

    TestCase {
        name: "WebEngineViewRunJavaScript"
        function test_runJavaScript() {
            var testTitle = "Title to test runJavaScript";
            runJavaScript("document.title = \"" + testTitle +"\"");
            _waitFor(function() { spy.count > 0; });
            compare(spy.count, 1);
            compare(webEngineView.title, testTitle);

            var testTitle2 = "Foobar"
            var testHtml = "<html><head><title>" + testTitle2 + "</title></head><body></body></html>";
            loadHtml(testHtml);
            waitForLoadSucceeded();
            var callbackCalled = false;
            runJavaScript("document.title", function(result) {
                    compare(result, testTitle2);
                    callbackCalled = true;
                });
            wait(100);
            verify(callbackCalled);
        }
    }
}
