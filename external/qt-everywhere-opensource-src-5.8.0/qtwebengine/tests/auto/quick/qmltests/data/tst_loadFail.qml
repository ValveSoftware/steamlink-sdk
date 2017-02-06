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
import QtWebEngine.testsupport 1.0

TestWebEngineView {
    id: webEngineView
    width: 400
    height: 300

    property var unavailableUrl: Qt.resolvedUrl("file_that_does_not_exist.html")
    property var loadRequestArray: []

    testSupport: WebEngineTestSupport {
        property var errorPageLoadStatus: null

        function waitForErrorPageLoadSucceeded() {
            var success = _waitFor(function() { return testSupport.errorPageLoadStatus == WebEngineView.LoadSucceededStatus })
            testSupport.errorPageLoadStatus = null
            return success
        }

        errorPage.onLoadingChanged: {
            errorPageLoadStatus = loadRequest.status

            loadRequestArray.push({
               "status": loadRequest.status,
               "url": loadRequest.url.toString(),
               "errorDomain": loadRequest.errorDomain,
               "isErrorPage": true
            })
        }
    }

    onLoadingChanged: {
        if (loadRequest.status == WebEngineView.LoadFailedStatus) {
            test.compare(loadRequest.url, unavailableUrl)
            test.compare(loadRequest.errorDomain, WebEngineView.InternalErrorDomain)
        }

        loadRequestArray.push({
           "status": loadRequest.status,
           "url": loadRequest.url.toString(),
           "errorDomain": loadRequest.errorDomain,
           "isErrorPage": false
        })
    }

    TestCase {
        id: test
        name: "WebEngineViewLoadFail"

        function test_fail() {
            WebEngine.settings.errorPageEnabled = false
            webEngineView.url = unavailableUrl
            verify(webEngineView.waitForLoadFailed())
        }

        function test_fail_url() {
            WebEngine.settings.errorPageEnabled = false
            var url = Qt.resolvedUrl("test1.html")
            webEngineView.url = url
            compare(webEngineView.url, url)
            verify(webEngineView.waitForLoadSucceeded())
            compare(webEngineView.url, url)

            webEngineView.url = unavailableUrl
            compare(webEngineView.url, unavailableUrl)
            verify(webEngineView.waitForLoadFailed())
            // When error page is disabled in case of LoadFail the entry of the unavailable page is not stored.
            // We expect the url of the previously loaded page here.
            compare(webEngineView.url, url)
        }

        function test_error_page() {
            WebEngine.settings.errorPageEnabled = true
            webEngineView.url = unavailableUrl

            // Loading of the error page must be successful
            verify(webEngineView.testSupport.waitForErrorPageLoadSucceeded())

            var loadRequest = null
            compare(loadRequestArray.length, 4)

            // Start to load unavailableUrl
            loadRequest = loadRequestArray[0]
            compare(loadRequest.status, WebEngineView.LoadStartedStatus)
            compare(loadRequest.errorDomain, WebEngineView.NoErrorDomain)
            compare(loadRequest.url, unavailableUrl)
            verify(!loadRequest.isErrorPage)

            // Loading of the unavailableUrl must fail
            loadRequest = loadRequestArray[1]
            compare(loadRequest.status, WebEngineView.LoadFailedStatus)
            compare(loadRequest.errorDomain, WebEngineView.InternalErrorDomain)
            compare(loadRequest.url, unavailableUrl)
            verify(!loadRequest.isErrorPage)

            // Start to load error page
            loadRequest = loadRequestArray[2]
            compare(loadRequest.status, WebEngineView.LoadStartedStatus)
            compare(loadRequest.errorDomain, WebEngineView.NoErrorDomain)
            compare(loadRequest.url, "data:text/html,chromewebdata")
            verify(loadRequest.isErrorPage)

            // Loading of the error page must be successful
            loadRequest = loadRequestArray[3]
            compare(loadRequest.status, WebEngineView.LoadSucceededStatus)
            compare(loadRequest.errorDomain, WebEngineView.NoErrorDomain)
            compare(loadRequest.url, "data:text/html,chromewebdata")
            verify(loadRequest.isErrorPage)

            compare(webEngineView.url, unavailableUrl)
            compare(webEngineView.title, unavailableUrl + " failed to load")
        }
    }
}
