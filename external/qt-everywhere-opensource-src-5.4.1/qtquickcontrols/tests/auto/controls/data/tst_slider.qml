/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0

Item {
    id: container
    width: 500
    height: 500

    TestCase {
        id: testCase
        name: "Tests_Slider"
        when:windowShown
        width:400
        height:400

        SignalSpy{
            id: spy
        }

        TestUtil {
            id: util
        }

        function test_vertical() {
            var slider = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {}', testCase, '');
            verify(slider.height < slider.width)

            slider.orientation = Qt.Vertical;
            verify(slider.height > slider.width)
            slider.destroy()
        }

        function test_minimumvalue() {
            var slider = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {}', testCase, '');

            slider.minimumValue = 5
            slider.maximumValue = 10
            slider.value = 2
            compare(slider.minimumValue, 5)
            compare(slider.value, 5)
            slider.destroy()
        }

        function test_maximumvalue() {
            var slider = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {}', testCase, '');

            slider.minimumValue = 5
            slider.maximumValue = 10
            slider.value = 15
            compare(slider.maximumValue, 10)
            compare(slider.value, 10)
            slider.destroy()
        }

        function test_rightLeftKeyPressed() {
            var slider = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {}', container, '');
            slider.forceActiveFocus()
            slider.maximumValue = 20
            slider.minimumValue = 0
            slider.value = 1
            slider.stepSize = 1
            keyPress(Qt.Key_Right)
            keyPress(Qt.Key_Right)
            compare(slider.value, 1 + slider.stepSize * 2)
            keyPress(Qt.Key_Left)
            compare(slider.value, 1 + slider.stepSize)

            slider.stepSize = 5
            slider.value = 15
            keyPress(Qt.Key_Right)
            compare(slider.value, 20)
            keyPress(Qt.Key_Right)
            compare(slider.value, 20)
            keyPress(Qt.Key_Left)
            compare(slider.value, 15)
            slider.destroy()
        }

        function test_mouseWheel() {
            var slider = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {}', container, '');
            slider.forceActiveFocus()
            slider.value = 0
            slider.maximumValue = 300
            slider.minimumValue = 0
            slider.stepSize = 2
            slider.width = 300

            var defaultScrollSpeed = 20.0
            var mouseStep = 15.0
            var deltatUnit = 8.0

            var mouseRatio = deltatUnit * mouseStep / defaultScrollSpeed;
            var sliderDeltaRatio = 1; //(slider.maximumValue - slider.minimumValue)/slider.width
            var ratio = mouseRatio / sliderDeltaRatio

            mouseWheel(slider, 5, 5, 20 * ratio, 0)
            compare(slider.value, 22)

            slider.maximumValue = 30
            slider.minimumValue = 0
            slider.stepSize = 1
            slider.value = 10
            sliderDeltaRatio = 0.1 //(slider.maximumValue - slider.minimumValue)/slider.width
            ratio = mouseRatio / sliderDeltaRatio

            compare(slider.value, 10)

            var previousValue = slider.value
            mouseWheel(slider, 5, 5, 6 * ratio, 0)
            compare(slider.value, Math.round(previousValue + 6))

            mouseWheel(slider, 5, 5, -6 * ratio, 0)
            compare(slider.value, previousValue)

            // Reach maximum
            slider.value = 0
            mouseWheel(slider, 5, 5, 40 * ratio, 0)
            compare(slider.value, slider.maximumValue)
            slider.destroy()
        }

        function test_activeFocusOnPress(){
            var control = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {x: 20; y: 20; width: 100; height: 50}', container, '')
            control.activeFocusOnPress = false
            verify(!control.activeFocus)
            mouseClick(control, 30, 30)
            verify(!control.activeFocus)
            control.activeFocusOnPress = true
            verify(!control.activeFocus)
            mousePress(control, 30, 30)
            verify(control.activeFocus)
            control.destroy()
        }

        function test_activeFocusOnTab() {
            if (!SystemInfo.tabAllWidgets)
                skip("This function doesn't support NOT iterating all.")

            var test_control = 'import QtQuick 2.2; \
            import QtQuick.Controls 1.2;            \
            Item {                                  \
                width: 200;                         \
                height: 200;                        \
                property alias control1: _control1; \
                property alias control2: _control2; \
                property alias control3: _control3; \
                Slider  {                           \
                    y: 20;                          \
                    id: _control1;                  \
                    activeFocusOnTab: true;         \
                }                                   \
                Slider  {                           \
                    y: 70;                          \
                    id: _control2;                  \
                    activeFocusOnTab: false;        \
                }                                   \
                Slider  {                           \
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

        function test_updateValueWhileDragging() {
            var controlString =
                    'import QtQuick 2.2 ;                     \
                     import QtQuick.Controls 1.2 ;            \
                     import QtQuick.Controls.Styles 1.1;      \
                     Slider {                                 \
                         width: 200 ;                         \
                         height : 50;                         \
                         style: SliderStyle{ handle: Item{ } }}'
            var control = Qt.createQmlObject(controlString, container, '')
            control.maximumValue = 200
            control.minimumValue = 0
            control.stepSize = 0.1
            control.value = 0
            container.forceActiveFocus()

            spy.target = control
            spy.signalName = "valueChanged"

            control.updateValueWhileDragging = false
            mouseDrag(control, 0,1, 100 , 0, Qt.LeftButton)
            compare(control.value, 100)
            compare(spy.count, 1)

            control.updateValueWhileDragging = true

            mouseDrag(control, 100,1, 80 , 0, Qt.LeftButton)
            compare(control.value, 180)
            compare(spy.count, 5)
            control.destroy()
        }

        function test_sliderOffset() {
            var control = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {x: 20; y: 20; width: 100; height: 50}', container, '')
            // Don't move slider value if mouse is inside handle regtion
            mouseMove(control, control.width/2, control.height/2)
            mouseClick(control, control.width/2, control.height/2)
            compare(control.value, 0.5)
            mouseMove(control, control.width/2 + 5, control.height/2)
            mouseClick(control, control.width/2 + 5, control.height/2)
            compare(control.value, 0.5)
            mouseMove(control, control.width/2 - 5, control.height/2)
            mouseClick(control, control.width/2 - 5, control.height/2)
            compare(control.value, 0.5)
            mouseMove(control, control.width/2 + 25, control.height/2)
            mouseClick(control, control.width/2 + 25, control.height/2)
            verify(control.value > 0.5)
            control.destroy()
        }

        function test_valueAndHandlePosition()
        {
            var slider = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {minimumValue: 0; maximumValue: 100; width: 100; height: 20; stepSize: 1}', container, '');
            slider.forceActiveFocus()
            slider.value = 0
            compare(slider.__handlePos, 0)
            slider.value = 50
            compare(slider.__handlePos, 50)
            slider.destroy()
        }

        function test_dragThreshold() {
            var control = Qt.createQmlObject('import QtQuick.Controls 1.2; Slider {x: 20; y: 20; width: 100; height: 50}', container, '')

            var pt = { x: control.width/2, y: control.height/2 }

            mousePress(control, pt.x, pt.y)
            compare(control.value, 0.5)

            // drag less than the threshold distance
            mouseMove(control, pt.x + Settings.dragThreshold - 1, pt.y)
            compare(control.value, 0.5)

            // drag over the threshold
            mouseMove(control, pt.x + Settings.dragThreshold + 1, pt.y)
            verify(control.value > 0.5)

            // move back close to the original press point, less than the threshold distance away
            mouseMove(control, pt.x - Settings.dragThreshold / 2, pt.y)
            verify(control.value < 0.5)

            control.destroy()
        }
    }
}
