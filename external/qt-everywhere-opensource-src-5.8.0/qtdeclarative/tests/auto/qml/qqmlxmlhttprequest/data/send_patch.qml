/****************************************************************************
**
** Copyright (C) 2015 Canonical Limited and/or its subsidiary(-ies).
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
