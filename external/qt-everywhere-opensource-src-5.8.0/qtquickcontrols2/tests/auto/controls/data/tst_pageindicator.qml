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
    name: "PageIndicator"

    Component {
        id: pageIndicator
        PageIndicator { }
    }

    function test_count() {
        var control = pageIndicator.createObject(testCase)
        verify(control)

        compare(control.count, 0)
        control.count = 3
        compare(control.count, 3)

        control.destroy()
    }

    function test_currentIndex() {
        var control = pageIndicator.createObject(testCase)
        verify(control)

        compare(control.currentIndex, 0)
        control.currentIndex = 5
        compare(control.currentIndex, 5)

        control.destroy()
    }

    function test_interactive() {
        var control = pageIndicator.createObject(testCase, {count: 5})
        verify(control)

        verify(!control.interactive)
        compare(control.currentIndex, 0)

        mouseClick(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.currentIndex, 0)

        control.interactive = true
        verify(control.interactive)

        mouseClick(control, control.width / 2, control.height / 2, Qt.LeftButton)
        compare(control.currentIndex, 2)

        // test also clicking outside delegates => the nearest should be selected
        control.padding = 10
        control.spacing = 10

        for (var i = 0; i < control.count; ++i) {
            var child = control.contentItem.children[i]
            for (var x = -2; x <= child.width + 2; ++x) {
                for (var y = -2; y <= child.height + 2; ++y) {
                    control.currentIndex = -1
                    compare(control.currentIndex, -1)

                    var pos = control.mapFromItem(child, x, y)
                    mouseClick(control, pos.x, pos.y, Qt.LeftButton)
                    compare(control.currentIndex, i)
                }
            }
        }

        control.destroy()
    }
}
