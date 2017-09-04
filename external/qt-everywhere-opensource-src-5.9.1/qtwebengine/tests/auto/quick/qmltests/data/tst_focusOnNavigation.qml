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

import QtQuick 2.5
import QtTest 1.0
import QtWebEngine 1.4

Item {
    id: container
    width: 500
    height: 300

    Row {
        id: row
        spacing: 1

        Rectangle {
            anchors.top: row.top
            anchors.topMargin: 7
            width: input.width
            height: input.height
            border.color: "black"
            border.width: 1

            TextInput {
                id: input
                width: 80
                height: 20
                verticalAlignment: TextInput.AlignVCenter
                horizontalAlignment: TextInput.AlignHCenter
                text: "Text"
            }
        }

        TestWebEngineView {
            id: webView
            width: 300
            height: 300
        }
    }

    TestCase {
        name: "WebEngineViewFocusOnNavigation"
        when: windowShown

        function test_focusOnNavigation_data() {
            return [
                {tag: "focusOnNavigation true", focusOnNavigation: true,
                                                viewReceivedFocus: true },
                {tag: "focusOnNavigation false", focusOnNavigation: false,
                                                 viewReceivedFocus: false },
            ]
        }

        function loadAndTriggerFocusAndCompare(data) {
            verify(webView.waitForLoadSucceeded());
            webView.setFocusToElement("input");
            compare(webView.activeFocus, data.viewReceivedFocus);
        }

        function test_focusOnNavigation(data) {
            // TextInput awlays has initial focus.
            input.forceActiveFocus();

            // Set focusOnNavigation property to current testing value.
            webView.settings.focusOnNavigationEnabled = data.focusOnNavigation;

            // Load the content, invoke javascript focus on the view, and check which widget has
            // focus.
            webView.loadHtml("<html><head><title>Title</title></head><body>Hello" +
                             "<input id=\"input\" type=\"text\"></body></html>");
            loadAndTriggerFocusAndCompare(data);

            // Load a different page, and check focus.
            webView.loadHtml("<html><head><title>Title</title></head><body>Hello 2" +
                             "<input id=\"input\" type=\"text\"></body></html>");
            loadAndTriggerFocusAndCompare(data);

            // Navigate to previous page in history, check focus.
            webView.triggerWebAction(WebEngineView.Back)
            loadAndTriggerFocusAndCompare(data);

            // Navigate to next page in history, check focus.
            webView.triggerWebAction(WebEngineView.Forward)
            loadAndTriggerFocusAndCompare(data);

            // Reload page, check focus.
            webView.triggerWebAction(WebEngineView.Reload)
            loadAndTriggerFocusAndCompare(data);

            // Reload page bypassing cache, check focus.
            webView.triggerWebAction(WebEngineView.ReloadAndBypassCache)
            loadAndTriggerFocusAndCompare(data);

            // Manually forcing focus on web view should work.
            webView.forceActiveFocus()
            compare(webView.activeFocus, true)
        }
    }
}
