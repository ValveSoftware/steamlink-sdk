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
    name: "Page"

    Component {
        id: page
        Page { }
    }

    Component {
        id: oneChildPage
        Page {
            Item {
                implicitWidth: 100
                implicitHeight: 30
            }
        }
    }

    Component {
        id: twoChildrenPage
        Page {
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
        id: contentPage
        Page {
            contentItem: Item {
                implicitWidth: 100
                implicitHeight: 30
            }
        }
    }

    Component {
        id: toolBar
        ToolBar { }
    }

    function test_defaults() {
        var control = page.createObject(testCase)
        verify(control)

        verify(control.contentItem)
        compare(control.header, null)
        compare(control.footer, null)

        control.destroy()
    }

    function test_empty() {
        var control = page.createObject(testCase)
        verify(control)

        verify(control.contentItem)
        compare(control.contentWidth, 0)
        compare(control.contentHeight, 0)

        control.destroy()
    }

    function test_oneChild() {
        var control = oneChildPage.createObject(testCase)
        verify(control)

        compare(control.contentWidth, 100)
        compare(control.contentHeight, 30)
        compare(control.implicitWidth, 100 + control.leftPadding + control.rightPadding)
        compare(control.implicitHeight, 30 + control.topPadding + control.bottomPadding)

        control.destroy()
    }

    function test_twoChildren() {
        var control = twoChildrenPage.createObject(testCase)
        verify(control)

        compare(control.contentWidth, 0)
        compare(control.contentHeight, 0)
        compare(control.implicitWidth, control.leftPadding + control.rightPadding)
        compare(control.implicitHeight, control.topPadding + control.bottomPadding)

        control.destroy()
    }

    function test_contentItem() {
        var control = contentPage.createObject(testCase)
        verify(control)

        compare(control.contentWidth, 100)
        compare(control.contentHeight, 30)
        compare(control.implicitWidth, 100 + control.leftPadding + control.rightPadding)
        compare(control.implicitHeight, 30 + control.topPadding + control.bottomPadding)

        control.destroy()
    }

    function test_layout() {
        var control = page.createObject(testCase, {width: 100, height: 100})
        verify(control)

        compare(control.width, 100)
        compare(control.height, 100)
        compare(control.contentItem.width, control.width)
        compare(control.contentItem.height, control.height)

        control.header = toolBar.createObject(control)
        compare(control.header.width, control.width)
        verify(control.header.height > 0)
        compare(control.contentItem.width, control.width)
        compare(control.contentItem.height, control.height - control.header.height)

        control.footer = toolBar.createObject(control)
        compare(control.footer.width, control.width)
        verify(control.footer.height > 0)
        compare(control.contentItem.width, control.width)
        compare(control.contentItem.height, control.height - control.header.height - control.footer.height)

        control.topPadding = 9
        control.leftPadding = 2
        control.rightPadding = 6
        control.bottomPadding = 7

        compare(control.header.x, 0)
        compare(control.header.y, 0)
        compare(control.header.width, control.width)
        verify(control.header.height > 0)

        compare(control.footer.x, 0)
        compare(control.footer.y, control.height - control.footer.height)
        compare(control.footer.width, control.width)
        verify(control.footer.height > 0)

        compare(control.contentItem.x, control.leftPadding)
        compare(control.contentItem.y, control.topPadding + control.header.height)
        compare(control.contentItem.width, control.availableWidth)
        compare(control.contentItem.height, control.availableHeight - control.header.height - control.footer.height)

        control.header.visible = false
        compare(control.contentItem.x, control.leftPadding)
        compare(control.contentItem.y, control.topPadding)
        compare(control.contentItem.width, control.availableWidth)
        compare(control.contentItem.height, control.availableHeight - control.footer.height)

        control.footer.visible = false
        compare(control.contentItem.x, control.leftPadding)
        compare(control.contentItem.y, control.topPadding)
        compare(control.contentItem.width, control.availableWidth)
        compare(control.contentItem.height, control.availableHeight)

        control.contentItem.implicitWidth = 50
        control.contentItem.implicitHeight = 60
        compare(control.implicitWidth, control.contentItem.implicitWidth + control.leftPadding + control.rightPadding)
        compare(control.implicitHeight, control.contentItem.implicitHeight + control.topPadding + control.bottomPadding)

        control.header.visible = true
        compare(control.implicitHeight, control.contentItem.implicitHeight + control.topPadding + control.bottomPadding
                                      + control.header.implicitHeight + control.spacing)

        control.footer.visible = true
        compare(control.implicitHeight, control.contentItem.implicitHeight + control.topPadding + control.bottomPadding
                                      + control.header.implicitHeight + control.footer.implicitHeight + 2 * control.spacing)

        control.header.implicitWidth = 150
        compare(control.implicitWidth, control.header.implicitWidth + control.leftPadding + control.rightPadding)

        control.footer.implicitWidth = 160
        compare(control.implicitWidth, control.footer.implicitWidth + control.leftPadding + control.rightPadding)

        control.destroy()
    }

    function test_spacing_data() {
        return [
            { tag: "content", header: false, content: true, footer: false },
            { tag: "header,content", header: true, content: true, footer: false },
            { tag: "content,footer", header: false, content: true, footer: true },
            { tag: "header,content,footer", header: true, content: true, footer: true },
            { tag: "header,footer", header: true, content: false, footer: true },
            { tag: "header", header: true, content: false, footer: false },
            { tag: "footer", header: false, content: false, footer: true },
        ]
    }

    function test_spacing(data) {
        var control = page.createObject(testCase, {spacing: 20, width: 100, height: 100})
        verify(control)

        control.contentItem.visible = data.content
        control.header = toolBar.createObject(control.contentItem, {visible: data.header})
        control.footer = toolBar.createObject(control.contentItem, {visible: data.footer})

        compare(control.header.x, 0)
        compare(control.header.y, 0)
        compare(control.header.width, control.width)
        verify(control.header.height > 0)

        compare(control.footer.x, 0)
        compare(control.footer.y, control.height - control.footer.height)
        compare(control.footer.width, control.width)
        verify(control.footer.height > 0)

        compare(control.contentItem.x, control.leftPadding)
        compare(control.contentItem.y, control.topPadding + (data.header ? control.header.height + control.spacing : 0))
        compare(control.contentItem.width, control.availableWidth)
        compare(control.contentItem.height, control.availableHeight
                                            - (data.header ? control.header.height + control.spacing : 0)
                                            - (data.footer ? control.footer.height + control.spacing : 0))

        control.destroy()
    }
}
