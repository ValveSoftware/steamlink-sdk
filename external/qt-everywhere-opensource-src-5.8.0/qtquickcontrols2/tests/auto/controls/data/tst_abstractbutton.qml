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
    width: 200
    height: 200
    visible: true
    when: windowShown
    name: "AbstractButton"

    Component {
        id: button
        AbstractButton {}
    }

    Component {
        id: item
        Item { }
    }

    Component {
        id: signalSpy
        SignalSpy { }
    }

    function test_text() {
        var control = button.createObject(testCase);
        verify(control);

        compare(control.text, "");
        control.text = "Button";
        compare(control.text, "Button");
        control.text = "";
        compare(control.text, "");

        control.destroy();
    }

    function test_baseline() {
        var control = button.createObject(testCase, {padding: 6})
        verify(control)
        compare(control.baselineOffset, 0)
        control.contentItem = item.createObject(control, {baselineOffset: 12})
        compare(control.baselineOffset, 18)
        control.destroy()
    }

    function test_implicitSize() {
        var control = button.createObject(testCase)
        verify(control)

        compare(control.implicitWidth, 0)
        compare(control.implicitHeight, 0)

        control.contentItem = item.createObject(control, {implicitWidth: 10, implicitHeight: 20})
        compare(control.implicitWidth, 10)
        compare(control.implicitHeight, 20)

        control.background = item.createObject(control, {implicitWidth: 20, implicitHeight: 30})
        compare(control.implicitWidth, 20)
        compare(control.implicitHeight, 30)

        control.padding = 100
        compare(control.implicitWidth, 210)
        compare(control.implicitHeight, 220)

        control.destroy()
    }

    function test_pressAndHold() {
        var control = button.createObject(testCase, {checkable: true})
        verify(control)

        var pressAndHoldSpy = signalSpy.createObject(control, {target: control, signalName: "pressAndHold"})
        verify(pressAndHoldSpy.valid)

        mousePress(control)
        pressAndHoldSpy.wait()
        compare(control.checked, false)

        mouseRelease(control)
        compare(control.checked, false)

        control.destroy()
    }
}
