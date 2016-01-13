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

TestCase {
    id: testCase
    name: "Tests_RangeModel"
    when: windowShown
    width:400
    height:400

    property var range

    SignalSpy {
        id: spy
        target: range
    }

    function init() {
        var component = Qt.createComponent("rangemodel/rangemodel.qml");
        compare(component.status, Component.Ready)
        range =  component.createObject(testCase);
        verify(range !== null, "created object is null")
    }

    function cleanup() {
        if (range !== 0)
            range.destroy()
        spy.clear()
    }

    function test_setminimumvalue() {
        spy.signalName = "minimumChanged"
        compare(spy.count, 0)
        compare(range.minimumValue, 0)
        range.minimumValue = 15
        compare(spy.count, 1)
        compare(range.minimumValue, 15)
    }

    function test_setmaximumvalue() {
        spy.signalName = "maximumChanged"
        compare(spy.count, 0)
        compare(range.maximumValue, 100)
        range.maximumValue = 35
        compare(spy.count, 1)
        compare(range.maximumValue, 35)
    }

    function test_setpositionatminimum() {
        spy.signalName = "positionAtMinimumChanged"
        compare(spy.count, 0)
        compare(range.positionAtMinimum, 0)
        range.positionAtMinimum = 15
        compare(spy.count, 1)
        compare(range.positionAtMinimum, 15)
    }

    function test_setpositionatmaximum() {
        spy.signalName = "positionAtMaximumChanged"
        compare(spy.count, 0)
        compare(range.positionAtMaximum, 100)
        range.positionAtMaximum = 35
        compare(spy.count, 1)
        compare(range.positionAtMaximum, 35)
    }

    function test_setvalue_data() {
        return [
                    { tag: "in range", value: 20, expected: 20 },
                    { tag: "below min", value: 5, expected: 10 },
                    { tag: "above max", value: 110, expected: 100 },
                ]
    }

    function test_setvalue(data) {
        spy.signalName = "valueChanged"
        compare(spy.count, 0)
        range.minimumValue = 10
        compare(range.minimumValue, 10)
        compare(range.maximumValue, 100)
        range.value = data.value
        compare(spy.count, 1)
        compare(range.value, data.expected)
    }

    function test_setinverted_data() {
        return [
                    { tag: "inverted false", value: false, posAtMin: 0, posAtMax: 100, expected: false },
                    { tag: "inverted true", value: true, posAtMin: 100, posAtMax: 0, expected: true },
                ]
    }

    function test_setinverted(data) {
        spy.signalName = "invertedChanged"
        compare(spy.count, 0)
        range.inverted = data.value
        if (range.inverted === true)
            compare(spy.count, 1)
        compare(range.inverted, data.expected )
        compare(range.positionForValue(0), data.posAtMin)
        compare(range.positionForValue(100), data.posAtMax)
        compare(range.valueForPosition(data.posAtMin), 0)
        compare(range.valueForPosition(data.posAtMax), 100)
    }

    function test_setstepsize() {
        spy.signalName = "stepSizeChanged"
        compare(spy.count, 0)
        compare(range.stepSize, 2)
        range.stepSize = 3
        compare(spy.count, 1)
        compare(range.stepSize, 3)
    }

    function test_setposition() {
        spy.signalName = "positionChanged"
        compare(spy.count, 0)
        compare(range.position, 40)
        range.position = 50
        compare(spy.count, 1)
        compare(range.position, 50)
        compare(range.valueForPosition(range.position), 50)
        range.positionAtMaximum = 200
        compare(spy.count, 2)
        compare(range.position, 100)
    }

    function test_bindings() {
        var component = Qt.createComponent("rangemodel/bindings.qml")
        compare(component.status, Component.Ready)
        var object = component.createObject(testCase)
        verify(object !== null, "created object is null")

        compare(object.range1.value, 50)
        compare(object.range1.minimumValue, 25)
        compare(object.range1.maximumValue, 75)

        compare(object.range2.value, 50)
        compare(object.range2.minimumValue, 25)
        compare(object.range2.maximumValue, 75)

        compare(object.range3.value, 50)
        compare(object.range3.minimumValue, 25)
        compare(object.range3.maximumValue, 75)
    }
}
