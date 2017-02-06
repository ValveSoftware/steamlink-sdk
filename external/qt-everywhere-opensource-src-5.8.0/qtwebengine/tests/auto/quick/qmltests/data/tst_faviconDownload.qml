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
import QtWebEngine 1.3

TestWebEngineView {
    id: webEngineView
    width: 200
    height: 400

    function removeFaviconProviderPrefix(url) {
        return url.toString().substring(16)
    }

    SignalSpy {
        id: iconChangedSpy
        target: webEngineView
        signalName: "iconChanged"
    }

    TestCase {
        id: test
        name: "WebEngineFaviconDownload"

        function init() {
            WebEngine.settings.autoLoadIconsForPage = true
            WebEngine.settings.touchIconsEnabled = false

            if (webEngineView.icon != '') {
                // If this is not the first test, then load a blank page without favicon, restoring the initial state.
                webEngineView.url = 'about:blank'
                verify(webEngineView.waitForLoadSucceeded())
                iconChangedSpy.wait()
            }

            iconChangedSpy.clear()
        }

        function cleanupTestCase() {
            WebEngine.settings.autoLoadIconsForPage = true
            WebEngine.settings.touchIconsEnabled = false
        }

        function test_downloadIconsDisabled_data() {
            return [
                   { tag: "misc", url: Qt.resolvedUrl("favicon-misc.html") },
                   { tag: "shortcut", url: Qt.resolvedUrl("favicon-shortcut.html") },
                   { tag: "single", url: Qt.resolvedUrl("favicon-single.html") },
                   { tag: "touch", url: Qt.resolvedUrl("favicon-touch.html") },
                   { tag: "unavailable", url: Qt.resolvedUrl("favicon-unavailable.html") },
            ];
        }

        function test_downloadIconsDisabled(row) {
            WebEngine.settings.autoLoadIconsForPage = false

            compare(iconChangedSpy.count, 0)

            webEngineView.url = row.url
            verify(webEngineView.waitForLoadSucceeded())

            compare(iconChangedSpy.count, 0)

            var iconUrl = webEngineView.icon
            compare(iconUrl, Qt.resolvedUrl(""))
        }

        function test_downloadTouchIconsEnabled_data() {
            return [
                   { tag: "misc", url: Qt.resolvedUrl("favicon-misc.html"), expectedIconUrl: Qt.resolvedUrl("icons/qt144.png") },
                   { tag: "shortcut", url: Qt.resolvedUrl("favicon-shortcut.html"), expectedIconUrl: Qt.resolvedUrl("icons/qt144.png") },
                   { tag: "single", url: Qt.resolvedUrl("favicon-single.html"), expectedIconUrl: Qt.resolvedUrl("icons/qt32.ico") },
                   { tag: "touch", url: Qt.resolvedUrl("favicon-touch.html"), expectedIconUrl: Qt.resolvedUrl("icons/qt144.png") },
            ];
        }

        function test_downloadTouchIconsEnabled(row) {
            WebEngine.settings.touchIconsEnabled = true

            compare(iconChangedSpy.count, 0)

            webEngineView.url = row.url
            verify(webEngineView.waitForLoadSucceeded())

            iconChangedSpy.wait()
            compare(iconChangedSpy.count, 1)

            var iconUrl = removeFaviconProviderPrefix(webEngineView.icon)
            compare(iconUrl, row.expectedIconUrl)
        }
    }
}

