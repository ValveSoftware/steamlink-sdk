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

    property var loadProgressArray: []

    onLoadProgressChanged: {
        loadProgressArray.push(webEngineView.loadProgress)
    }

    TestCase {
        name: "WebEngineViewLoadProgress"

        function test_loadProgress() {
            compare(webEngineView.loadProgress, 0)
            loadProgressArray = []

            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            // Test whether the chromium emits progress numbers in ascending order
            var loadProgressMin = 0
            for (var i in loadProgressArray) {
                var loadProgress = loadProgressArray[i]
                verify(loadProgressMin <= loadProgress)
                loadProgressMin = loadProgress
            }

            // The progress must be 100% at the end
            compare(loadProgressArray[loadProgressArray.length - 1], 100)
        }
    }
}
