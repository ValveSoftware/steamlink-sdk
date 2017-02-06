/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
    property bool xmlTest: false
    property bool typeTest: false

    function checkXML(document)
    {
        if (document.xmlVersion != "1.0")
            return;

        if (document.xmlEncoding != "utf-8")
            return;

        if (document.documentElement == null)
            return;

        var multistatus = document.documentElement;
        if (multistatus.nodeName != "multistatus")
            return;

        if (multistatus.namespaceUri != "DAV:")
            return;

        var multistatusChildTags = [ "response"  ];
        for (var node = 0; node < multistatus.childNodes.length; ++node) {
            if (multistatus.childNodes[node].nodeName != multistatusChildTags[node])
                return;
        }

        var response = multistatus.childNodes[0];
        var responseChildTags =  [ "href", "propstat" ];
        for (var node = 0; node < response.childNodes.length; ++node) {
            var nodeName = response.childNodes[node].nodeName;
            if (nodeName != responseChildTags[node])
                return;

            var nodeValue = response.childNodes[node].childNodes[0].nodeValue;
            if ((nodeName == "href") && (nodeValue != "/container/"))
                return;
        }

        var propstat = response.childNodes[1];
        var propstatChildTags = ["prop", "status"];
        for (var node = 0; node < propstat.childNodes.length; ++node) {
            var nodeName = propstat.childNodes[node].nodeName;
            if (nodeName != propstatChildTags[node])
                return;

            var nodeValue = propstat.childNodes[node].childNodes[0].nodeValue;
            if ((nodeName == "status") && (nodeValue != "HTTP/1.1 200 OK"))
                return;
        }

        var prop = propstat.childNodes[0];
        var propChildTags = [ "bigbox", "author", "creationdate", "displayname", "resourcetype", "supportedlock" ];
        for (var node = 0; node < prop.childNodes.length; ++node) {
            var nodeName = prop.childNodes[node].nodeName;
            if (nodeName != propChildTags[node])
                return;

            if (nodeName == "bigbox") {
                if (prop.childNodes[node].childNodes.length != 1)
                    return;

                var boxType = prop.childNodes[node].childNodes[0];
                if (boxType.nodeName != "BoxType")
                    return;
                if (boxType.childNodes[0].nodeValue != "Box type A")
                    return;
            }

            if (nodeName == "author") {
                if (prop.childNodes[node].childNodes.length != 1)
                    return;

                var boxType = prop.childNodes[node].childNodes[0];
                if (boxType.nodeName != "Name")
                    return;
                if (boxType.childNodes[0].nodeValue != "Hadrian")
                    return;
            }

            if (nodeName == "creationdate") {
                if (prop.childNodes[node].childNodes.length != 1)
                    return;

                if (prop.childNodes[node].childNodes[0].nodeValue != "1997-12-01T17:42:21-08:00")
                    return;
            }

            if (nodeName == "displayname") {
                if (prop.childNodes[node].childNodes.length != 1)
                    return;

                if (prop.childNodes[node].childNodes[0].nodeValue != "Example collection")
                    return;
            }

            if (nodeName == "resourcetpye") {
                if (prop.childNodes[node].childNodes.length != 1)
                    return;

                if (prop.childNodes[node].childNodes[0].nodeValue != "collection")
                    return;
            }

            if (nodeName == "supportedlock") {
                if (prop.childNodes[node].childNodes.length != 2)
                    return;

                var lockEntry1 = prop.childNodes[node].childNodes[0];
                if (lockEntry1.nodeName != "lockentry")
                    return;
                if (lockEntry1.childNodes.length != 2)
                    return;
                if (lockEntry1.childNodes[0].nodeName != "lockscope")
                    return;
                if (lockEntry1.childNodes[0].childNodes[0].nodeName != "exclusive")
                    return;
                if (lockEntry1.childNodes[1].nodeName != "locktype")
                    return;
                if (lockEntry1.childNodes[1].childNodes[0].nodeName != "write")
                    return;

                var lockEntry2 = prop.childNodes[node].childNodes[1];
                if (lockEntry2.nodeName != "lockentry")
                    return;
                if (lockEntry2.childNodes.length != 2)
                    return;
                if (lockEntry2.childNodes[0].nodeName != "lockscope")
                    return;
                if (lockEntry2.childNodes[0].childNodes[0].nodeName != "shared")
                    return;
                if (lockEntry2.childNodes[1].nodeName != "locktype")
                    return;
                if (lockEntry2.childNodes[1].childNodes[0].nodeName != "write")
                    return;
            }
        }

        xmlTest = true;
    }

    Component.onCompleted: {

        var request = new XMLHttpRequest();
        request.open("PROPFIND", url);
        request.responseType = "document";
        request.setRequestHeader("Depth", "1");

        request.onreadystatechange = function() {
            if (request.readyState == XMLHttpRequest.DONE) {
                checkXML(request.response);
                typeTest = (request.responseType == "document");
            }
        }

        var requestBody = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" +
                          "<D:propfind xmlns:D=\"DAV:\">\n" +
                              "<D:allprop/>\n" +
                          "</D:propfind>\n"
       request.send(requestBody);
    }
}

