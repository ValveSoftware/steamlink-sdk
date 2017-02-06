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
import QtQuick.Controls 1.4
import QtQuick.Controls.Private 1.0
import QtQuick.Extras 1.4

TestCase {
    id: testCase
    name: "Tests_StatusIndicator"
    visible: true
    when: windowShown
    width: 400
    height: 400

    property var indicator

    function cleanup() {
        if (indicator)
            indicator.destroy();
    }

    function test_instance() {
        indicator = Qt.createQmlObject("import QtQuick.Extras 1.4; StatusIndicator { }", testCase, "");
        verify(indicator, "StatusIndicator: failed to create an instance")
        verify(indicator.__style);
    }

    function test_active_data() {
        if (Settings.styleName === "Flat") {
            return [
                { tag: "active", active: true, expectedColor: { r: 18, g: 136, b: 203 } },
                { tag: "inactive", active: false, expectedColor: { r: 179, g: 179, b: 179 } }
            ];
        }

        return [
            { tag: "active", active: true, expectedColor: { r: 255, g: 99, b: 99 } },
            { tag: "inactive", active: false, expectedColor: { r: 52, g: 52, b: 52 } }
        ];
    }

    function test_active(data) {
        indicator = Qt.createQmlObject("import QtQuick.Extras 1.4; StatusIndicator { }", testCase, "");
        verify(indicator);
        compare(indicator.active, false);

        indicator.active = data.active;
        // Color is slightly different on some platforms/machines, like Windows.
        var lenience = Settings.styleName === "Flat" ? 0 : 2;

        waitForRendering(indicator);
        var image = grabImage(indicator);
        fuzzyCompare(image.red(indicator.width / 2, indicator.height / 2), data.expectedColor.r, lenience);
        fuzzyCompare(image.green(indicator.width / 2, indicator.height / 2), data.expectedColor.g, lenience);
        fuzzyCompare(image.blue(indicator.width / 2, indicator.height / 2), data.expectedColor.b, lenience);
    }

    function test_color() {
        var flatStyle = Settings.styleName === "Flat";

        indicator = Qt.createQmlObject("import QtQuick.Extras 1.4; StatusIndicator { }", testCase, "");
        verify(indicator);
        compare(indicator.color, flatStyle ? "#1288cb" : "#ff0000");

        // The base style uses a gradient for its color, so pure blue will not be pure blue.
        var expectedR = flatStyle ? 0 : 99;
        var expectedG = flatStyle ? 0 : 99;
        var expectedB = flatStyle ? 255 : 255;
        var lenience = flatStyle ? 0 : 2;

        indicator.active = true;
        indicator.color = "#0000ff";
        waitForRendering(indicator);
        var image = grabImage(indicator);
        fuzzyCompare(image.red(indicator.width / 2, indicator.height / 2), expectedR, lenience);
        fuzzyCompare(image.green(indicator.width / 2, indicator.height / 2), expectedG, lenience);
        fuzzyCompare(image.blue(indicator.width / 2, indicator.height / 2), expectedB, lenience);
    }

    function test_baseStyleHasOuterShadow() {
        if (Settings.styleName !== "Base")
            return;

        indicator = Qt.createQmlObject("import QtQuick.Extras 1.4; StatusIndicator { }", testCase, "");
        verify(indicator);

        // There should be a "shadow" here...
        waitForRendering(indicator);
        var image = grabImage(indicator);
        verify(image.pixel(indicator.width / 2, 1) !== Qt.rgba(1, 1, 1, 1));
    }
}
