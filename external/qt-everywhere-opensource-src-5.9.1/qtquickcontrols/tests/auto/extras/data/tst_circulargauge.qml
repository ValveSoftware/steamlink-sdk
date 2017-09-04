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
import "TestUtils.js" as TestUtils

TestCase {
    id: testcase
    name: "Tests_CircularGauge"
    when: windowShown
    width: 400
    height: 400

    function test_instance() {
        var gauge = Qt.createQmlObject('import QtQuick.Extras 1.4; CircularGauge { }', testcase, '');
        verify (gauge, "CircularGauge: failed to create an instance")
        verify(gauge.__style)
        gauge.destroy()
    }

    property Component tickmark: Rectangle {
        objectName: "tickmark" + styleData.index
        implicitWidth: 2
        implicitHeight: 6
        color: "#c8c8c8"
    }

    property Component minorTickmark: Rectangle {
        objectName: "minorTickmark" + styleData.index
        implicitWidth: 1
        implicitHeight: 3
        color: "#c8c8c8"
    }

    property Component tickmarkLabel: Text {
        objectName: "tickmarkLabel" + styleData.index
        text: styleData.value
        color: "#c8c8c8"
    }

    function test_tickmarksVisible() {
        var gauge = Qt.createQmlObject("import QtQuick.Extras 1.4; CircularGauge { }", testcase, "");
        verify(gauge, "CircularGauge: failed to create an instance");

        gauge.__style.tickmark = tickmark;
        gauge.__style.minorTickmark = minorTickmark;
        gauge.__style.tickmarkLabel = tickmarkLabel;
        verify(TestUtils.findChild(gauge, "tickmark0"));
        verify(TestUtils.findChild(gauge, "minorTickmark0"));
        verify(TestUtils.findChild(gauge, "tickmarkLabel0"));

        gauge.tickmarksVisible = false;
        verify(!TestUtils.findChild(gauge, "tickmark0"));
        verify(!TestUtils.findChild(gauge, "minorTickmark0"));
        verify(!TestUtils.findChild(gauge, "tickmarkLabel0"));

        gauge.destroy();
    }
}
