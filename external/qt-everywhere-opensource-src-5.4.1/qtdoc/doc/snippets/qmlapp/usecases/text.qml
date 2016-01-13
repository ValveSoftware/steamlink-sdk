/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the documentation of the Qt Toolkit.
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

//![0]
import QtQuick 2.3

Item {
    id: root
    width: 480
    height: 320

    Rectangle {
        color: "#272822"
        width: 480
        height: 320
    }

    Column {
        spacing: 20

        Text {
            text: 'I am the very model of a modern major general!'

            // color can be set on the entire element with this property
            color: "yellow"

        }

        Text {
            // For text to wrap, a width has to be explicitly provided
            width: root.width

            // This setting makes the text wrap at word boundaries when it goes past the width of the Text object
            wrapMode: Text.WordWrap

            // You can use \ to escape quotation marks, or to add new lines (\n). Use \\ to get a \ in the string
            text: 'I am the very model of a modern major general. I\'ve information vegetable, animal and mineral. I know the kings of england and I quote the fights historical; from Marathon to Waterloo in order categorical.'

            // color can be set on the entire element with this property
            color: "white"

        }

        Text {
            text: 'I am the very model of a modern major general!'

            // color can be set on the entire element with this property
            color: "yellow"

            // font properties can be set effciently on the whole string at once
            font { family: 'Courier'; pixelSize: 20; italic: true; capitalization: Font.SmallCaps }

        }

        Text {
            // HTML like markup can also be used
            text: '<font color="white">I am the <b>very</b> model of a modern <i>major general</i>!</font>'

            // This could also be written font { pointSize: 14 }. Both syntaxes are valid.
            font.pointSize: 14

            // StyledText format supports fewer tags, but is more efficient than RichText
            textFormat: Text.StyledText

        }
    }
}
//![0]
