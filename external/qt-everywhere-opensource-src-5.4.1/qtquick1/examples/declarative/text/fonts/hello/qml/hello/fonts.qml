/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 1.0

Rectangle {
    property string myText: "The quick brown fox jumps over the lazy dog."

    width: 800; height: 480
    color: "steelblue"

    FontLoader { id: fixedFont; name: "Courier" }
    FontLoader { id: localFont; source: "fonts/tarzeau_ocr_a.ttf" }
    FontLoader { id: webFont; source: "http://www.princexml.com/fonts/steffmann/Starburst.ttf" }

    Column {
        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
        spacing: 15

        Text {
            text: myText
            color: "lightsteelblue"
            width: parent.width
            elide: Text.ElideRight
            font.family: "Times"; font.pointSize: 42
        }
        Text {
            text: myText
            color: "lightsteelblue"
            width: parent.width
            elide: Text.ElideLeft
            font { family: "Times"; pointSize: 42; capitalization: Font.AllUppercase }
        }
        Text {
            text: myText
            color: "lightsteelblue"
            width: parent.width
            elide: Text.ElideMiddle
            font { family: fixedFont.name; pointSize: 42; weight: Font.Bold; capitalization: Font.AllLowercase }
        }
        Text {
            text: myText
            color: "lightsteelblue"
            width: parent.width
            elide: Text.ElideRight
            font { family: fixedFont.name; pointSize: 42; italic: true; capitalization: Font.SmallCaps }
        }
        Text {
            text: myText
            color: "lightsteelblue"
            width: parent.width
            elide: Text.ElideLeft
            font { family: localFont.name; pointSize: 42; capitalization: Font.Capitalize }
        }
        Text {
            text: {
                if (webFont.status == FontLoader.Ready) myText
                else if (webFont.status == FontLoader.Loading) "Loading..."
                else if (webFont.status == FontLoader.Error) "Error loading font"
            }
            color: "lightsteelblue"
            width: parent.width
            elide: Text.ElideMiddle
            font.family: webFont.name; font.pointSize: 42
        }
    }
}
