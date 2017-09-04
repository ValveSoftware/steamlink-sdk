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
    id: testCase
    name: "Tests_Common"
    visible: true
    when: windowShown
    width: 400
    height: 400

    property var control

    function init_data() {
         return [
             { tag: "CircularGauge" },
             { tag: "DelayButton" },
             { tag: "Dial" },
             { tag: "Gauge" },
             { tag: "StatusIndicator" },
             { tag: "ToggleButton" },
             { tag: "Tumbler", qml: "import QtQuick.Extras 1.4; Tumbler { TumblerColumn { model: 10 } }" },
             { tag: "PieMenu", qml: "import QtQuick.Controls 1.1; import QtQuick.Extras 1.4;"
                + "PieMenu { visible: true; MenuItem { text: 'foo' } }"}
         ];
    }

    function init() {
        if (Qt.platform.os === "windows")
            skip("QTBUG-53123");
    }

    function cleanup() {
        if (control)
            control.destroy();
    }

    function test_resize(data) {
        var qml = data.qml ? data.qml : "import QtQuick.Extras 1.4; " + data.tag + " { }";
        control = Qt.createQmlObject(qml, testCase, "");

        var resizeAnimation = Qt.createQmlObject("import QtQuick 2.2; NumberAnimation {}", control, "");
        resizeAnimation.target = control;
        resizeAnimation.properties = "width,height";
        resizeAnimation.duration = 100;
        resizeAnimation.to = 0;
        resizeAnimation.start();
        // Shouldn't get any warnings.
        tryCompare(resizeAnimation, "running", false);
    }
}
