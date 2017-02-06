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

    property var loadRequestArray: []

    onLoadingChanged: {
        loadRequestArray.push({
            "status": loadRequest.status,
            "url": loadRequest.url,
            "activeUrl": webEngineView.url
        });
    }

    function clear() {
        // Reset loadStatus for waitForLoadSucceded
        webEngineView.loadStatus = null;
        loadRequestArray = [];
    }

    TestCase {
        name: "WebEngineViewLoadUrl"
        when: windowShown

        function init() {
            webEngineView.clear();
        }

        function test_loadIgnoreEmptyUrl() {
            var url = Qt.resolvedUrl("test1.html");
            webEngineView.url = url;
            verify(webEngineView.waitForLoadSucceeded());
            compare(loadRequestArray[0].status, WebEngineView.LoadStartedStatus);
            compare(loadRequestArray[1].status, WebEngineView.LoadSucceededStatus);
            compare(loadRequestArray.length, 2);
            compare(webEngineView.url, url);
            webEngineView.clear();

            var lastUrl = webEngineView.url;
            webEngineView.url = "";
            wait(1000);
            compare(loadRequestArray.length, 0);
            compare(webEngineView.url, lastUrl);
            webEngineView.clear();

            var aboutBlank = "about:blank";
            webEngineView.url = aboutBlank;
            verify(webEngineView.waitForLoadSucceeded());
            compare(loadRequestArray[0].status, WebEngineView.LoadStartedStatus);
            compare(loadRequestArray[1].status, WebEngineView.LoadSucceededStatus);
            compare(loadRequestArray.length, 2);
            compare(webEngineView.url, aboutBlank);
            webEngineView.clear();

            // It shouldn't interrupt any ongoing load when an empty url is used.
            var watchProgress = true;
            var handleLoadProgress = function() {
                if (webEngineView.loadProgress != 100) {
                    webEngineView.url = "";
                    watchProgress = false;
                }
            }
            webEngineView.loadProgressChanged.connect(handleLoadProgress);
            webEngineView.url = url;
            verify(webEngineView.waitForLoadSucceeded());
            compare(loadRequestArray[0].status, WebEngineView.LoadStartedStatus);
            compare(loadRequestArray[1].status, WebEngineView.LoadSucceededStatus);
            compare(loadRequestArray.length, 2);
            verify(!watchProgress);
            compare(webEngineView.url, url);
            webEngineView.loadProgressChanged.disconnect(handleLoadProgress);
            webEngineView.clear();
        }

        function test_urlProperty() {
            WebEngine.settings.errorPageEnabled = false;

            var loadRequest = null;

            // Test succeeded load
            var url = Qt.resolvedUrl("test1.html");
            webEngineView.url = url;
            tryCompare(loadRequestArray, "length", 2);

            loadRequest = loadRequestArray[0];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.activeUrl, url);
            loadRequest = loadRequestArray[1];
            compare(loadRequest.status, WebEngineView.LoadSucceededStatus);
            compare(loadRequest.activeUrl, url);
            webEngineView.clear();

            // Test failed load
            var bogusSite = "http://www.somesitethatdoesnotexist.abc/";
            webEngineView.url = bogusSite;
            tryCompare(loadRequestArray, "length", 2);

            loadRequest = loadRequestArray[0];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.activeUrl, bogusSite);
            loadRequest = loadRequestArray[1];
            compare(loadRequest.status, WebEngineView.LoadFailedStatus);
            compare(loadRequest.activeUrl, url);
            webEngineView.clear();

            // Test page redirection
            var redirectUrl = Qt.resolvedUrl("redirect.html");
            webEngineView.url = redirectUrl;
            tryCompare(loadRequestArray, "length", 4);

            loadRequest = loadRequestArray[0];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.activeUrl, redirectUrl);
            loadRequest = loadRequestArray[1];
            compare(loadRequest.status, WebEngineView.LoadSucceededStatus);
            compare(loadRequest.activeUrl, redirectUrl);
            loadRequest = loadRequestArray[2];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.activeUrl, redirectUrl);
            loadRequest = loadRequestArray[3];
            compare(loadRequest.status, WebEngineView.LoadSucceededStatus);
            compare(loadRequest.activeUrl, url);
            webEngineView.clear();

            // Test clicking on a hyperlink
            var linkUrl = Qt.resolvedUrl("link.html");
            webEngineView.url = linkUrl;
            tryCompare(loadRequestArray, "length", 2);

            loadRequest = loadRequestArray[0];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.activeUrl, linkUrl);
            loadRequest = loadRequestArray[1];
            compare(loadRequest.status, WebEngineView.LoadSucceededStatus);
            compare(loadRequest.activeUrl, linkUrl);
            webEngineView.clear();

            var lastUrl = webEngineView.url;
            mouseClick(webEngineView, 10, 10, Qt.LeftButton, Qt.NoModifiers, 50);
            tryCompare(loadRequestArray, "length", 2);

            loadRequest = loadRequestArray[0];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.url, url);
            compare(loadRequest.activeUrl, lastUrl);
            loadRequest = loadRequestArray[1];
            compare(loadRequest.status, WebEngineView.LoadSucceededStatus);
            compare(loadRequest.url, url);
            compare(loadRequest.activeUrl, url);
            webEngineView.clear();
        }

        function test_loadDataUrl() {
            WebEngine.settings.errorPageEnabled = false;

            var loadRequest = null;

            // Test load of a data URL
            var dataUrl = "data:text/html,foo";
            webEngineView.url = dataUrl;
            tryCompare(loadRequestArray, "length", 2);

            loadRequest = loadRequestArray[0];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.activeUrl, dataUrl);
            loadRequest = loadRequestArray[1];
            compare(loadRequest.status, WebEngineView.LoadSucceededStatus);
            compare(loadRequest.activeUrl, dataUrl);
            webEngineView.clear();

            // Test loadHtml after a failed load
            var aboutBlank = "about:blank";
            webEngineView.url = aboutBlank; // Reset from previous test
            verify(webEngineView.waitForLoadSucceeded());
            webEngineView.clear();

            var bogusSite = "http://www.somesitethatdoesnotexist.abc/";
            var handleLoadFailed = function(loadRequest) {
                if (loadRequest.status == WebEngineView.LoadFailedStatus) {
                    // loadHtml constructs data URL
                    webEngineView.loadHtml("load failed", bogusSite);
                    compare(loadRequest.url, bogusSite);
                }
            }
            webEngineView.loadingChanged.connect(handleLoadFailed);
            webEngineView.url = bogusSite
            tryCompare(loadRequestArray, "length", 4);
            webEngineView.loadingChanged.disconnect(handleLoadFailed);

            loadRequest = loadRequestArray[0];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.activeUrl, bogusSite);
            loadRequest = loadRequestArray[1];
            compare(loadRequest.status, WebEngineView.LoadFailedStatus);
            // Since the load did not succeed the active url is the
            // URL of the previous successful load.
            compare(loadRequest.activeUrl, aboutBlank);
            loadRequest = loadRequestArray[2];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.activeUrl, aboutBlank);
            loadRequest = loadRequestArray[3];
            compare(loadRequest.status, WebEngineView.LoadSucceededStatus);
            compare(loadRequest.activeUrl, bogusSite);
            webEngineView.clear();
        }

        function test_QTBUG_56661() {
            var url = Qt.resolvedUrl("test1.html");

            // Warm up phase
            webEngineView.url = url;
            verify(webEngineView.waitForLoadSucceeded());

            // Load data URL
            var dataUrl = "data:text/html,foo";
            webEngineView.url = dataUrl;
            verify(webEngineView.waitForLoadSucceeded());

            // WebEngine should not try to execute user scripts in the
            // render frame of the warm up phase otherwise the renderer
            // crashes.
            webEngineView.url = url;
            verify(webEngineView.waitForLoadSucceeded());
        }

        function test_stopStatus() {
            var loadRequest = null;

            var handleLoadStarted = function(loadRequest) {
                if (loadRequest.status == WebEngineView.LoadStartedStatus)
                    webEngineView.stop();
            }
            webEngineView.loadingChanged.connect(handleLoadStarted);
            var url = Qt.resolvedUrl("test1.html");
            webEngineView.url = url;
            tryCompare(loadRequestArray, "length", 2);
            webEngineView.loadingChanged.disconnect(handleLoadStarted);

            loadRequest = loadRequestArray[0];
            compare(loadRequest.status, WebEngineView.LoadStartedStatus);
            compare(loadRequest.url, url);
            compare(loadRequest.activeUrl, url);
            loadRequest = loadRequestArray[1];
            compare(loadRequest.status, WebEngineView.LoadStoppedStatus);
            compare(loadRequest.url, url);
            compare(loadRequest.activeUrl, url);
            webEngineView.clear();
        }
    }
}
