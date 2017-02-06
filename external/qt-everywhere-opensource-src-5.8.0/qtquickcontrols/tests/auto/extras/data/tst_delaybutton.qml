/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

import QtTest 1.0
import QtQuick 2.1

TestCase {
    id: testcase
    name: "Tests_DelayButton"
    visible: true
    when: windowShown
    width: 400
    height: 400

    function test_instance() {
        var button = Qt.createQmlObject('import QtQuick.Extras 1.4; DelayButton { }', testcase, '')
        verify (button, "DelayButton: failed to create an instance")
        verify(button.__style)
        verify(!button.checked)
        verify(!button.pressed)
        button.destroy()
    }

    SignalSpy {
        id: activationSpy
        signalName: "activated"
    }

    function test_activation_data() {
        return [
            { tag: "delayed", delay: 1 },
            { tag: "immediate", delay: 0 },
            { tag: "negative", delay: -1 }
        ]
    }

    function test_activation(data) {
        var button = Qt.createQmlObject('import QtQuick.Extras 1.4; DelayButton { }', testcase, '')
        verify (button, "DelayButton: failed to create an instance")
        button.delay = data.delay

        activationSpy.clear()
        activationSpy.target = button
        verify(activationSpy.valid)

        // press and hold to activate
        mousePress(button, button.width / 2, button.height / 2)
        verify(button.pressed)
        var immediate = data.delay <= 0
        if (!immediate)
            activationSpy.wait()
        compare(activationSpy.count, 1)

        // release
        mouseRelease(button, button.width / 2, button.height / 2)
        verify(!button.pressed)
        compare(activationSpy.count, 1)

        button.destroy()
    }

    SignalSpy {
        id: progressSpy
        signalName: "progressChanged"
    }

    function test_progress() {
        var button = Qt.createQmlObject('import QtQuick.Extras 1.4; DelayButton { delay: 1 }', testcase, '')
        verify (button, "DelayButton: failed to create an instance")

        progressSpy.target = button
        verify(progressSpy.valid)

        compare(button.progress, 0.0)
        mousePress(button, button.width / 2, button.height / 2)
        tryCompare(button, "progress", 1.0)
        verify(progressSpy.count > 0)

        button.destroy()
    }

    SignalSpy {
        id: checkSpy
        signalName: "checkedChanged"
    }

    function test_checked_data() {
        return [
            { tag: "delayed", delay: 1 },
            { tag: "immediate", delay: 0 },
            { tag: "negative", delay: -1 }
        ]
    }

    function test_checked(data) {
        var button = Qt.createQmlObject('import QtQuick.Extras 1.4; DelayButton { }', testcase, '')
        verify (button, "DelayButton: failed to create an instance")
        button.delay = data.delay

        var checkCount = 0

        checkSpy.clear()
        checkSpy.target = button
        verify(checkSpy.valid)
        verify(!button.checked)

        // press and hold to check
        mousePress(button, button.width / 2, button.height / 2)
        verify(button.pressed)
        var immediate = data.delay <= 0
        compare(button.checked, immediate)
        if (!immediate)
            tryCompare(button, "checked", true)
        compare(checkSpy.count, ++checkCount)

        // release
        mouseRelease(button, button.width / 2, button.height / 2)
        verify(!button.pressed)
        verify(button.checked)
        compare(checkSpy.count, checkCount)

        // press to uncheck immediately
        mousePress(button, button.width / 2, button.height / 2)
        verify(button.pressed)
        verify(!button.checked)
        compare(checkSpy.count, ++checkCount)

        // release
        mouseRelease(button, button.width / 2, button.height / 2)
        verify(!button.pressed)
        verify(!button.checked)
        compare(checkSpy.count, checkCount)

        button.destroy()
    }

    function test_programmaticCheck() {
        var button = Qt.createQmlObject("import QtQuick.Extras 1.4; DelayButton {}", testcase, "");
        verify(button, "DelayButton: failed to create an instance");

        checkSpy.clear();
        checkSpy.target = button;
        verify(!button.checked);

        button.checked = true;
        compare(button.progress, 1);

        button.checked = false;
        compare(button.progress, 0);

        button.destroy();
    }

    function test_largeText() {
        // Should be no binding loop warnings.
        var button = Qt.createQmlObject("import QtQuick.Extras 1.4; DelayButton { "
            + "anchors.centerIn: parent; text: 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' }", testcase, "");
        verify(button, "DelayButton: failed to create an instance");
        button.destroy();
    }
}
