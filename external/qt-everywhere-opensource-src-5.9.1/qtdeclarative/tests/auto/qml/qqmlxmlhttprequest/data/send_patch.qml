/****************************************************************************
**
** Copyright (C) 2017 Canonical Limited and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

QtObject {
    property string url

    property bool dataOK: false
    property bool headerOK: false

    Component.onCompleted: {
        var x = new XMLHttpRequest;
        x.open("PATCH", url);
        x.setRequestHeader("Accept-Language","en-US");
        x.setRequestHeader("If-Match","\"ETagNumber\"");

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                headerOK = (x.getResponseHeader("Content-Location") == "/qqmlxmlhttprequest.cpp") &&
                    (x.getResponseHeader("ETag") == "\"ETagNumber\"") &&
                    (x.status == "204");
            } else if (x.readyState == XMLHttpRequest.DONE) {
                dataOK = (x.responseText === "");
            }
        }

        var body = "--- a/qqmlxmlhttprequest.cpp\n" +
            "+++ b/qqmlxmlhttprequest.cpp\n" +
            "@@ -1238,11 +1238,13 @@\n" +
            "-    } else if (m_method == QLatin1String(\"OPTIONS\")) {\n" +
            "+    } else if (m_method == QLatin1String(\"OPTIONS\") ||\n" +
            "+            (m_method == QLatin1String(\"PATCH\"))) {\n"

        x.send(body);
    }
}
