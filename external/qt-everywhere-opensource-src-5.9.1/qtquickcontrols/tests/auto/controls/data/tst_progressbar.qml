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

import QtQuick 2.2
import QtTest 1.0
import QtQuickControlsTests 1.0

Item {
    id: container
    width: 400
    height: 400

TestCase {
    id: testCase
    name: "Tests_ProgressBar"
    when:windowShown
    width:400
    height:400

    function test_minimumvalue() {
        var progressBar = Qt.createQmlObject('import QtQuick.Controls 1.2; ProgressBar {}', testCase, '');

        progressBar.minimumValue = 5
        progressBar.maximumValue = 10
        progressBar.value = 2
        compare(progressBar.minimumValue, 5)
        compare(progressBar.value, 5)

        progressBar.minimumValue = 7
        compare(progressBar.value, 7)
        progressBar.destroy()
    }

    function test_maximumvalue() {
        var progressBar = Qt.createQmlObject('import QtQuick.Controls 1.2; ProgressBar {}', testCase, '');

        progressBar.minimumValue = 5
        progressBar.maximumValue = 10
        progressBar.value = 15
        compare(progressBar.maximumValue, 10)
        compare(progressBar.value, 10)

        progressBar.maximumValue = 8
        compare(progressBar.value, 8)
        progressBar.destroy()
    }

    function test_invalidMinMax() {
        var progressBar = Qt.createQmlObject('import QtQuick.Controls 1.2; ProgressBar {}', testCase, '');

        // minimumValue has priority over maximum if they are inconsistent

        progressBar.minimumValue = 10
        progressBar.maximumValue = 10
        compare(progressBar.value, progressBar.minimumValue)

        progressBar.value = 17
        compare(progressBar.value, progressBar.minimumValue)

        progressBar.maximumValue = 5
        compare(progressBar.value, progressBar.minimumValue)

        progressBar.value = 12
        compare(progressBar.value, progressBar.minimumValue)

        var progressBar2 = Qt.createQmlObject('import QtQuick.Controls 1.2; ProgressBar {minimumValue: 10; maximumValue: 4; value: 5}', testCase, '');
        compare(progressBar.value, progressBar.minimumValue)
        progressBar.destroy()
        progressBar2.destroy()
    }

    function test_initialization_order()
    {
        var progressBar = Qt.createQmlObject("import QtQuick.Controls 1.2; ProgressBar {maximumValue: 100; value: 50}",
                                         testCase, '')
        compare(progressBar.value, 50);

        var progressBar2 = Qt.createQmlObject("import QtQuick.Controls 1.2; ProgressBar {" +
                                         "value: 50; maximumValue: 100}",
                                         testCase, '')
        compare(progressBar2.value, 50);

        var progressBar3 = Qt.createQmlObject("import QtQuick.Controls 1.2; ProgressBar { minimumValue: -50 ; value:-10}",
                                         testCase, '')
        compare(progressBar3.value, -10);

        var progressBar4 = Qt.createQmlObject("import QtQuick.Controls 1.2; ProgressBar { value:-10; minimumValue: -50}",
                                         testCase, '')
        compare(progressBar4.value, -10);
        progressBar.destroy()
        progressBar2.destroy()
        progressBar3.destroy()
        progressBar4.destroy()
    }

    function test_activeFocusOnTab() {
        if (Qt.styleHints.tabFocusBehavior != Qt.TabFocusAllControls)
            skip("This function doesn't support NOT iterating all.")

        var test_control = 'import QtQuick 2.2; \
        import QtQuick.Controls 1.2;            \
        Item {                                  \
            width: 200;                         \
            height: 200;                        \
            property alias control1: _control1; \
            property alias control2: _control2; \
            property alias control3: _control3; \
            ProgressBar  {                      \
                y: 20;                          \
                id: _control1;                  \
                activeFocusOnTab: true;         \
            }                                   \
            ProgressBar  {                      \
                y: 70;                          \
                id: _control2;                  \
                activeFocusOnTab: false;        \
            }                                   \
            ProgressBar  {                      \
                y: 120;                         \
                id: _control3;                  \
                activeFocusOnTab: true;         \
            }                                   \
        }                                       '

        var control = Qt.createQmlObject(test_control, container, '')
        control.control1.forceActiveFocus()
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(!control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(control.control3.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(control.control3.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)

        control.control2.activeFocusOnTab = true
        control.control3.activeFocusOnTab = false
        keyPress(Qt.Key_Tab)
        verify(!control.control1.activeFocus)
        verify(control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab)
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(!control.control1.activeFocus)
        verify(control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        keyPress(Qt.Key_Tab, Qt.ShiftModifier)
        verify(control.control1.activeFocus)
        verify(!control.control2.activeFocus)
        verify(!control.control3.activeFocus)
        control.destroy()
    }
}
}
