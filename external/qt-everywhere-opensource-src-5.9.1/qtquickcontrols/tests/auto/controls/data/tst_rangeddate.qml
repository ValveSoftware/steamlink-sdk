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
import QtQuick.Controls.Private 1.0
import QtTest 1.0

Item {
    id: container

    TestCase {
        id: testcase
        name: "Tests_RangedDate"
        when: true

        property var rangedDate
        readonly property string importsStr: "import QtQuick.Controls 1.2; import QtQuick.Controls.Private 1.0; "

        function init() {
            rangedDate = Qt.createQmlObject(importsStr + " RangedDate {}", container, "");
        }

        function test_defaultConstruction() {
            // rangedDate.date should be the current date.
            compare(rangedDate.date.getTime(), new Date(Date.now()).setHours(0, 0, 0, 0));
        }

        function test_minMax() {
            skip("QTBUG-36846");

            rangedDate.minimumDate = CalendarUtils.minimumCalendarDate;
            rangedDate.maximumDate = CalendarUtils.maximumCalendarDate;

            compare(rangedDate.minimumDate.getTime(), CalendarUtils.minimumCalendarDate.getTime());
            compare(rangedDate.maximumDate.getTime(), CalendarUtils.maximumCalendarDate.getTime());
        }

        function test_constructionPropertyOrder() {
            // All values are valid; fine.
            rangedDate = Qt.createQmlObject(importsStr + " RangedDate { "
                + "date: new Date(1900, 0, 2); "
                + "minimumDate: new Date(1900, 0, 1); "
                + "maximumDate: new Date(1900, 0, 3); "
                + " }", container, "");
            compare(rangedDate.date.getTime(), new Date(1900, 0, 2).getTime());
            compare(rangedDate.minimumDate.getTime(), new Date(1900, 0, 1).getTime());
            compare(rangedDate.maximumDate.getTime(), new Date(1900, 0, 3).getTime());

            // All values are the same; doesn't make sense, but is fine [1].
            rangedDate = Qt.createQmlObject(importsStr + " RangedDate { "
                + "date: new Date(1900, 0, 1);"
                + "minimumDate: new Date(1900, 0, 1);"
                + "maximumDate: new Date(1900, 0, 1);"
                + " }", container, "");
            compare(rangedDate.date.getTime(), new Date(1900, 0, 1).getTime());
            compare(rangedDate.minimumDate.getTime(), new Date(1900, 0, 1).getTime());
            compare(rangedDate.maximumDate.getTime(), new Date(1900, 0, 1).getTime());

            // date is lower than min - should be clamped to min.
            rangedDate = Qt.createQmlObject(importsStr + " RangedDate { "
                + "date: new Date(1899, 0, 1);"
                + "minimumDate: new Date(1900, 0, 1);"
                + "maximumDate: new Date(1900, 0, 1);"
                + " }", container, "");
            compare(rangedDate.date.getTime(), new Date(1900, 0, 1).getTime());
            compare(rangedDate.minimumDate.getTime(), new Date(1900, 0, 1).getTime());
            compare(rangedDate.maximumDate.getTime(), new Date(1900, 0, 1).getTime());

            // date is higher than max - should be clamped to max.
            rangedDate = Qt.createQmlObject(importsStr + " RangedDate { "
                + "date: new Date(1900, 0, 2);"
                + "minimumDate: new Date(1900, 0, 1);"
                + "maximumDate: new Date(1900, 0, 1);"
                + " }", container, "");
            compare(rangedDate.date.getTime(), new Date(1900, 0, 1).getTime());
            compare(rangedDate.minimumDate.getTime(), new Date(1900, 0, 1).getTime());
            compare(rangedDate.maximumDate.getTime(), new Date(1900, 0, 1).getTime());

            // If the order of property construction is undefined (as it should be if it's declarative),
            // then is min considered higher than max or max lower than min? Which should be changed?

            // For now, max will always be the one that's changed. It will be set to min,
            // as min may already be the largest possible date (See [1]).
            rangedDate = Qt.createQmlObject(importsStr + " RangedDate { "
                + "date: new Date(1900, 0, 1);"
                + "minimumDate: new Date(1900, 0, 2);"
                + "maximumDate: new Date(1900, 0, 1);"
                + " }", container, "");
            compare(rangedDate.date.getTime(), new Date(1900, 0, 2).getTime());
            compare(rangedDate.minimumDate.getTime(), new Date(1900, 0, 2).getTime());
            compare(rangedDate.maximumDate.getTime(), new Date(1900, 0, 2).getTime());

            // [1] Do we want to enforce min and max being different? E.g. if min
            // is (1900, 0, 1) and max is (1900, 0, 1), max should be set (1900, 0, 2).
            // Another e.g.: if min is the largest possible date and max is the same,
            // min should be set to the previous day, and vice versa.
            // Currently, I'm assuming that it's fine for them to be the same,
            // even if it means date can only ever be one value.
        }

        // "Flickering" is the effect that changing the min/max has on the date;
        // setting the minimum to be higher than the date will force the date to
        // be changed, and the same applies to the maximum.
        function test_flickering() {
            //    MIN   DATE    MAX
            // [ 1990 | 1995 | 1999 ]
            rangedDate.date = new Date(1995, 0, 1);
            compare(rangedDate.date.getTime(), new Date(1995, 0, 1).getTime());
            rangedDate.minimumDate = new Date(1990, 0, 1);
            compare(rangedDate.minimumDate.getTime(), new Date(1990, 0, 1).getTime());
            rangedDate.maximumDate = new Date(1999, 0, 1);
            compare(rangedDate.maximumDate.getTime(), new Date(1999, 0, 1).getTime());

            //    MIN   DATE    MAX
            // [ 1996 | 1996 | 1999 ]
            rangedDate.minimumDate = new Date(1996, 0, 1);
            compare(rangedDate.date.getTime(), new Date(1996, 0, 1).getTime());
            compare(rangedDate.minimumDate.getTime(), new Date(1996, 0, 1).getTime());

            //    MIN   DATE    MAX
            // [ 1990 | 1996 | 1999 ]
            rangedDate.minimumDate = new Date(1990, 0, 1);
            compare(rangedDate.date.getTime(), new Date(1996, 0, 1).getTime());
            compare(rangedDate.minimumDate.getTime(), new Date(1990, 0, 1).getTime());
        }

        function test_nullValues() {
            var expected = new Date(rangedDate.date);
            rangedDate.date = undefined;
            compare(rangedDate.date.getTime(), expected.getTime());

            expected = new Date(rangedDate.minimumDate);
            rangedDate.minimumDate = undefined;
            compare(rangedDate.minimumDate.getTime(), expected.getTime());

            expected = new Date(rangedDate.maximumDate);
            rangedDate.maximumDate = undefined;
            compare(rangedDate.maximumDate.getTime(), expected.getTime());
        }
    }
}
