/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtTest 1.0
import QtLocation 5.3
import "utils.js" as Utils

TestCase {
    id: testCase

    name: "Ratings"

    Ratings { id: emptyRatings }

    function test_empty() {
        compare(emptyRatings.average, 0.0);
        compare(emptyRatings.maximum, 0.0);
        compare(emptyRatings.count, 0);
    }

    Ratings {
        id: qmlRatings

        average: 3.5
        maximum: 5.0
        count: 7
    }

    function test_qmlConstructedRatings() {
        compare(qmlRatings.average, 3.5);
        compare(qmlRatings.maximum, 5.0);
        compare(qmlRatings.count, 7);
    }

    Ratings {
        id: testRatings
    }

    function test_setAndGet_data() {
        return [
            { tag: "average", property: "average", signal: "averageChanged", value: 4.5, reset: 0.0 },
            { tag: "maximum", property: "maximum", signal: "maximumChanged", value: 5.0, reset: 0.0 },
            { tag: "count", property: "count", signal: "countChanged", value: 10, reset: 0 },
        ];
    }

    function test_setAndGet(data) {
        Utils.testObjectProperties(testCase, testRatings, data);
    }
}
