/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

import QtQuick 2.0
import QtTest 1.1

Item {
    id: top

    TextInput {
        id: emptyText
        height: 20
        width: 50
    }

    TextInput {
        id: txtfamily
        text: "Hello world!"
        font.family: "Courier"
        height: 20
        width: 50
    }

    TextInput {
        id: txtcolor
        text: "Hello world!"
        color: "red"
        height: 20
        width: 50
    }

    TextInput {
        id: txtentry
        text: ""
        height: 20
        width: 50
    }

    TextInput {
        id: txtfunctions
        text: "The quick brown fox"
        height: 20
        width: 50
    }

    TextInput {
        id: txtclipboard
        text: "The quick brown fox jumped over the lazy dog"
        height: 20
        width: 50
    }

    TextInput {
        id: txtintvalidator
        text: ""
        height: 20
        width: 50
        validator: IntValidator { id: iv; top: 20; bottom: 10; }
    }

    TextInput {
        id: txtdoublevalidator
        text: ""
        height: 20
        width: 50
        validator: DoubleValidator { id: dv; top: 2.0; bottom: 1.0; }
    }

    TextInput {
        id: txtregexpvalidator
        text: ""
        height: 20
        width: 50
        validator: RegExpValidator { id: rv; regExp: /[a-z]{3}/ }
    }



    TestCase {
        name: "TextInput"
        when: windowShown

        function test_empty() {
            compare(emptyText.text, "")
        }

        function test_family() {
            compare(txtfamily.font.family, "Courier")
            txtfamily.font.family = "Helvetica";
            compare(txtfamily.font.family, "Helvetica")
        }

        function test_color() {
            compare(txtcolor.color, "#ff0000")
            txtcolor.color = "blue";
            compare(txtcolor.color, "#0000ff")
        }

        function test_textentry() {
            txtentry.focus = true;
            compare(txtentry.text, "")
            keyClick(Qt.Key_H)
            keyClick(Qt.Key_E)
            keyClick(Qt.Key_L)
            keyClick(Qt.Key_L)
            keyClick(Qt.Key_O)
            keyClick(Qt.Key_Space)
            keyClick(Qt.Key_W)
            keyClick(Qt.Key_O)
            keyClick(Qt.Key_R)
            keyClick(Qt.Key_L)
            keyClick(Qt.Key_D)
            compare(txtentry.text, "hello world")
        }

        function test_select_insert() {
            compare(txtfunctions.getText(4,9), "quick")
            txtfunctions.select(4,9);
            compare(txtfunctions.selectedText, "quick")
            txtfunctions.insert(4, "very ")
            compare(txtfunctions.text, "The very quick brown fox")
            txtfunctions.deselect();
            compare(txtfunctions.selectedText, "")
            txtfunctions.text = "Qt";
            txtfunctions.insert(txtfunctions.text.length, " ")
            compare(txtfunctions.text, "Qt ");
            txtfunctions.insert(txtfunctions.text.length, "quick")
            compare(txtfunctions.text, "Qt quick");
            txtfunctions.cursorPosition = txtfunctions.text.length;
            txtfunctions.selectWord();
            compare(txtfunctions.selectedText, "quick")
            txtfunctions.selectAll();
            compare(txtfunctions.selectedText, "Qt quick")
            txtfunctions.deselect();
            compare(txtfunctions.selectedText, "")
        }

        function test_clipboard() {
            if (typeof(txtclipboard.copy) !== "function"
             || typeof(txtclipboard.paste) !== "function"
             || typeof(txtclipboard.cut) !== "function") {
                skip("Clipboard is not supported on this platform.")
            }
            txtclipboard.select(4,10);
            txtclipboard.cut();
            compare(txtclipboard.text, "The brown fox jumped over the lazy dog")
            txtclipboard.select(30,35)
            txtclipboard.paste();
            compare(txtclipboard.text, "The brown fox jumped over the quick dog")
            txtclipboard.text = "Qt ";
            txtclipboard.cursorPosition = txtclipboard.text.length;
            txtclipboard.paste();
            compare(txtclipboard.text, "Qt quick ");
            txtclipboard.cursorPosition = txtclipboard.text.length-1;
            txtclipboard.selectWord();
            compare(txtclipboard.selectedText, "quick")
            txtclipboard.copy();
            txtclipboard.cursorPosition = txtclipboard.text.length;
            txtclipboard.paste();
            compare(txtclipboard.text, "Qt quick quick");
        }

        function test_intvalidators_data() {
            return [
                {
                    tag: "toolow",
                    testnumber: "5",
                    acceptable: false
                },
                {
                    tag: "toohigh",
                    testnumber: "50",
                    acceptable: false
                },
                {
                    tag: "onlowerbounds",
                    testnumber: "10",
                    acceptable: true
                },
                {
                    tag: "onupperbounds",
                    testnumber: "20",
                    acceptable: true
                },
                {
                    tag: "middle",
                    testnumber: "15",
                    acceptable: true
                },
                {
                    tag: "negativemiddle",
                    testnumber: "-15",
                    acceptable: false
                }
            ]

        }

        function test_intvalidators(row) {
            compare(txtintvalidator.validator.top, 20)
            compare(txtintvalidator.validator.bottom, 10)
            txtintvalidator.text = row.testnumber;
            compare(txtintvalidator.acceptableInput, row.acceptable)
        }

        function test_doublevalidators_data() {
            return [
                {
                    tag: "toolow",
                    testnumber: "0.5",
                    acceptable: false
                },
                {
                    tag: "toohigh",
                    testnumber: "2.5",
                    acceptable: false
                },
                {
                    tag: "onlowerbounds",
                    testnumber: "1.0",
                    acceptable: true
                },
                {
                    tag: "onupperbounds",
                    testnumber: "2.0",
                    acceptable: true
                },
                {
                    tag: "middle",
                    testnumber: "1.5",
                    acceptable: true
                },
                {
                    tag: "negativemiddle",
                    testnumber: "-1.5",
                    acceptable: false
                }
            ]

        }

        function test_doublevalidators(row) {
            txtdoublevalidator.validator.locale = "C"
            compare(txtdoublevalidator.validator.top, 2.0)
            compare(txtdoublevalidator.validator.bottom, 1.0)
            txtdoublevalidator.text = row.testnumber;
            compare(txtdoublevalidator.acceptableInput, row.acceptable)
        }

        function test_regexpvalidators_data() {
            return [
                {
                    tag: "toolow",
                    testtext: "ab",
                    acceptable: false
                },
                {
                    tag: "toohigh",
                    testtext: "abcd",
                    acceptable: false
                },
                {
                    tag: "acceptable",
                    testtext: "abc",
                    acceptable: true
                }
            ]

        }

        function test_regexpvalidators(row) {
            compare(txtregexpvalidator.validator.regExp, /[a-z]{3}/)
            txtregexpvalidator.text = row.testtext;
            compare(txtregexpvalidator.acceptableInput, row.acceptable)
        }
    }
}
