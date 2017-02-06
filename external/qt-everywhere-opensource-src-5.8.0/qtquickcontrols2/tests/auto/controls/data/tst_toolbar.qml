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

import QtQuick 2.2
import QtTest 1.0
import QtQuick.Controls 2.1

TestCase {
    id: testCase
    width: 400
    height: 400
    visible: true
    when: windowShown
    name: "ToolBar"

    Component {
        id: toolBar
        ToolBar { }
    }

    Component {
        id: oneChildBar
        ToolBar {
            Item {
                implicitWidth: 100
                implicitHeight: 30
            }
        }
    }

    Component {
        id: twoChildrenBar
        ToolBar {
            Item {
                implicitWidth: 100
                implicitHeight: 30
            }
            Item {
                implicitWidth: 200
                implicitHeight: 60
            }
        }
    }

    Component {
        id: contentBar
        ToolBar {
            contentItem: Item {
                implicitWidth: 100
                implicitHeight: 30
            }
        }
    }

    function test_empty() {
        var control = toolBar.createObject(testCase)
        verify(control)

        verify(control.contentItem)
        compare(control.contentWidth, 0)
        compare(control.contentHeight, 0)

        control.destroy()
    }

    function test_oneChild() {
        var control = oneChildBar.createObject(testCase)
        verify(control)

        compare(control.contentWidth, 100)
        compare(control.contentHeight, 30)
        verify(control.implicitWidth >= 100)
        verify(control.implicitHeight >= 30)

        control.destroy()
    }

    function test_twoChildren() {
        var control = twoChildrenBar.createObject(testCase)
        verify(control)

        compare(control.contentWidth, 0)
        compare(control.contentHeight, 0)
        verify(control.implicitWidth >= 0)
        verify(control.implicitHeight >= 0)

        control.destroy()
    }

    function test_contentItem() {
        var control = contentBar.createObject(testCase)
        verify(control)

        compare(control.contentWidth, 100)
        compare(control.contentHeight, 30)
        verify(control.implicitWidth >= 100)
        verify(control.implicitHeight >= 30)

        control.destroy()
    }
}
