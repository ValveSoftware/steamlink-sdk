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
import QtQml 2.2
import Qt.labs.calendar 1.0

TestCase {
    id: testCase
    name: "CalendarModel"

    Component {
        id: calendarModel
        CalendarModel { }
    }

    Component {
        id: instantiator
        Instantiator {
            model: CalendarModel {
                from: new Date(2016, 0, 1)
                to: new Date(2016, 11, 31)
            }
            QtObject {
                readonly property int month: model.month
                readonly property int year: model.year
            }
        }
    }

    function test_indices_data() {
        return [
            { tag: "2013", from: "2013-01-01", to: "2013-12-31", count: 12 },
            { tag: "2016", from: "2016-01-01", to: "2016-03-31", count: 3 }
        ]
    }

    function test_indices(data) {
        var model = calendarModel.createObject(testCase, {from: data.from, to: data.to})
        verify(model)

        compare(model.count, data.count)

        var y = parseInt(data.tag)
        for (var m = 0; m < 12; ++m) {
            compare(model.yearAt(m), y)
            compare(model.indexOf(y, m), m)
            compare(model.indexOf(new Date(y, m, 1)), m)
            compare(model.monthAt(m), m)
        }

        model.destroy()
    }

    function test_invalid() {
        var model = calendarModel.createObject(testCase)
        verify(model)

        compare(model.indexOf(-1, -1), -1)
        compare(model.indexOf(new Date(-1, -1, -1)), -1)

        model.destroy()
    }

    function test_instantiator() {
        var inst = instantiator.createObject(testCase)
        verify(inst)

        compare(inst.count, 12)
        for (var m = 0; m < inst.count; ++m) {
            compare(inst.objectAt(m).month, m)
            compare(inst.objectAt(m).year, 2016)
        }

        inst.destroy()
    }
}
