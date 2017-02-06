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
    name: "ProgressBar"

    Component {
        id: progressBar
        ProgressBar { }
    }

    function test_value() {
        var control = progressBar.createObject(testCase)
        verify(control)

        compare(control.value, 0.0)
        control.value = 0.5
        compare(control.value, 0.5)
        control.value = 1.0
        compare(control.value, 1.0)
        control.value = -1.0
        compare(control.value, 0.0)
        control.value = 2.0
        compare(control.value, 1.0)

        control.destroy()
    }

    function test_range() {
        var control = progressBar.createObject(testCase, {from: 0, to: 100, value: 50})
        verify(control)

        compare(control.from, 0)
        compare(control.to, 100)
        compare(control.value, 50)
        compare(control.position, 0.5)

        control.value = 1000
        compare(control.value, 100)
        compare(control.position, 1)

        control.value = -1
        compare(control.value, 0)
        compare(control.position, 0)

        control.from = 25
        compare(control.from, 25)
        compare(control.value, 25)
        compare(control.position, 0)

        control.to = 75
        compare(control.to, 75)
        compare(control.value, 25)
        compare(control.position, 0)

        control.value = 50
        compare(control.value, 50)
        compare(control.position, 0.5)

        control.destroy()
    }

    function test_inverted() {
        var control = progressBar.createObject(testCase, {from: 1.0, to: -1.0})
        verify(control)

        compare(control.from, 1.0)
        compare(control.to, -1.0)
        compare(control.value, 0.0)
        compare(control.position, 0.5)

        control.value = 2.0
        compare(control.value, 1.0)
        compare(control.position, 0.0)

        control.value = -2.0
        compare(control.value, -1.0)
        compare(control.position, 1.0)

        control.value = 0.0
        compare(control.value, 0.0)
        compare(control.position, 0.5)

        control.destroy()
    }

    function test_position() {
        var control = progressBar.createObject(testCase)
        verify(control)

        compare(control.value, 0)
        compare(control.position, 0)

        control.value = 0.25
        compare(control.value, 0.25)
        compare(control.position, 0.25)

        control.value = 0.75
        compare(control.value, 0.75)
        compare(control.position, 0.75)

        control.destroy()
    }

    function test_visualPosition() {
        var control = progressBar.createObject(testCase)
        verify(control)

        compare(control.value, 0)
        compare(control.visualPosition, 0)

        control.value = 0.25
        compare(control.value, 0.25)
        compare(control.visualPosition, 0.25)

        // RTL locale
        control.locale = Qt.locale("ar_EG")
        compare(control.visualPosition, 0.75)

        // RTL locale + LayoutMirroring
        control.LayoutMirroring.enabled = true
        compare(control.visualPosition, 0.75)

        // LTR locale + LayoutMirroring
        control.locale = Qt.locale("en_US")
        compare(control.visualPosition, 0.75)

        // LTR locale
        control.LayoutMirroring.enabled = false
        compare(control.visualPosition, 0.25)

        // LayoutMirroring
        control.LayoutMirroring.enabled = true
        compare(control.visualPosition, 0.75)

        control.destroy()
    }

    function test_indeterminate() {
        var control = progressBar.createObject(testCase)
        verify(control)
        compare(control.indeterminate, false)

        wait(100)
        control.indeterminate = true
        wait(100)
        // Shouldn't crash...
        control.indeterminate = false

        control.destroy()
    }
}
