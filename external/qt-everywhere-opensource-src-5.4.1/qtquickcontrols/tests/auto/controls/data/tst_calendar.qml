/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
import QtQuick.Controls.Private 1.0
import QtTest 1.0

Item {
    id: container
    width: 300
    height: 300

    TestCase {
        id: testcase
        name: "Tests_Calendar"
        when: windowShown
        readonly property int navigationBarHeight: calendar !== undefined ? calendar.__panel.navigationBarItem.height : 0
        readonly property int dayOfWeekHeaderRowHeight: calendar !== undefined ? calendar.__panel.dayOfWeekHeaderRow.height : 0
        readonly property int firstDateCellX: 0
        readonly property int firstDateCellY: navigationBarHeight + dayOfWeekHeaderRowHeight
        readonly property int previousMonthButtonX: navigationBarHeight / 2
        readonly property int previousMonthButtonY: navigationBarHeight / 2
        readonly property int nextMonthButtonX: calendar ? calendar.width - navigationBarHeight / 2 : 0
        readonly property int nextMonthButtonY: navigationBarHeight / 2

        property var calendar

        SignalSpy {
            id: hoveredSignalSpy
        }

        SignalSpy {
            id: pressedSignalSpy
        }

        SignalSpy {
            id: clickedSignalSpy
        }

        SignalSpy {
            id: releasedSignalSpy
        }

        SignalSpy {
            id: pressAndHoldSignalSpy
        }

        function init() {
            calendar = Qt.createQmlObject("import QtQuick.Controls 1.2; " +
                " Calendar { }", container, "");
            calendar.width = 300;
            calendar.height = 300;
            waitForRendering(calendar);
        }

        function cleanup() {
            hoveredSignalSpy.clear();
            pressedSignalSpy.clear();
            clickedSignalSpy.clear();
            releasedSignalSpy.clear();
            pressAndHoldSignalSpy.clear();
        }

        function toPixelsX(cellPosX) {
            var cellRect = calendar.__style.__cellRectAt(cellPosX);
            return firstDateCellX + cellRect.x + cellRect.width / 2;
        }

        function toPixelsY(cellPosY) {
            var cellRect = calendar.__style.__cellRectAt(cellPosY * calendar.__panel.columns);
            return firstDateCellY + cellRect.y + cellRect.height / 2;
        }

        function test_defaultConstructed() {
            calendar.minimumDate = new Date(1, 0, 1);
            calendar.maximumDate = new Date(4000, 0, 1);

            compare(calendar.minimumDate, new Date(1, 0, 1));
            compare(calendar.maximumDate, new Date(4000, 0, 1));
            compare(calendar.selectedDate, new Date(new Date().setHours(0, 0, 0, 0)));
            compare(calendar.frameVisible, true);
            compare(calendar.dayOfWeekFormat, Locale.ShortFormat);
            compare(calendar.__locale, Qt.locale());
        }

        function test_setAfterConstructed() {
            calendar.minimumDate = new Date(1900, 0, 1);
            calendar.maximumDate = new Date(1999, 11, 31);
            calendar.selectedDate = new Date(1980, 0, 1);
            calendar.frameVisible = false;
            calendar.dayOfWeekFormat = Locale.NarrowFormat;
            calendar.__locale = Qt.locale("de_DE");

            compare(calendar.minimumDate, new Date(1900, 0, 1));
            compare(calendar.maximumDate, new Date(1999, 11, 31));
            compare(calendar.selectedDate, new Date(1980, 0, 1));
            compare(calendar.frameVisible, false);
            compare(calendar.__locale, Qt.locale("de_DE"));
        }

        function test_selectedDate() {
            calendar.minimumDate = new Date(2012, 0, 1);
            calendar.maximumDate = new Date(2013, 0, 1);

            // Equal to minimumDate date.
            calendar.selectedDate = new Date(calendar.minimumDate);
            compare(calendar.selectedDate, calendar.minimumDate);

            // Outside minimum date.
            calendar.selectedDate.setDate(calendar.selectedDate.getDate() - 1);
            compare(calendar.selectedDate, calendar.minimumDate);

            // Equal to maximum date.
            calendar.selectedDate = new Date(calendar.maximumDate);
            compare(calendar.selectedDate, calendar.maximumDate);

            // Outside maximumDate date.
            calendar.selectedDate.setDate(calendar.selectedDate.getDate() - 1);
            compare(calendar.selectedDate, calendar.maximumDate);

            // Should not change.
            calendar.selectedDate = undefined;
            compare(calendar.selectedDate, calendar.maximumDate);
        }

        // Should be able to use the full JS date range.
        function test_minMaxJsDateRange() {
            skip("QTBUG-36846");

            calendar.minimumDate = CalendarUtils.minimumCalendarDate;
            calendar.maximumDate = CalendarUtils.maximumCalendarDate;

            calendar.selectedDate = CalendarUtils.minimumCalendarDate;
            compare(calendar.selectedDate.getTime(), CalendarUtils.minimumCalendarDate.getTime());

            calendar.selectedDate = CalendarUtils.maximumCalendarDate;
            compare(calendar.selectedDate.getTime(), CalendarUtils.maximumCalendarDate.getTime());
        }

        function test_minMaxUndefined() {

            var expected = new Date(calendar.minimumDate);
            calendar.minimumDate = undefined;
            compare(calendar.minimumDate, expected);

            expected = new Date(calendar.maximumDate);
            calendar.maximumDate = undefined;
            compare(calendar.maximumDate, expected);
        }

        function test_keyNavigation() {
            calendar.forceActiveFocus();
            calendar.selectedDate = new Date(2013, 0, 1);
            // Set this to a certain locale, because days will be in different
            // places depending on the system locale of the host machine.
            calendar.__locale = Qt.locale("en_GB");

            /*         January 2013                    December 2012
                 M   T   W   T   F   S   S        M   T   W   T   F   S   S
                31  [1]  2   3   4   5   6       26  27  28  29  30   1   2
                 7   8   9  10  11  12  13        3   4   5   6   7   8   9
                14  15  16  17  18  19  20  ==>  10  11  12  13  14  15  16
                21  22  23  24  25  26  27       17  18  19  20  21  22  23
                28  29  30  31   1   2   3       24  25  26  27  28  29  30
                 4   5   6   7   8   9  10      [31]  1   2   3   4   5   6 */
            keyPress(Qt.Key_Left);
            compare(calendar.selectedDate, new Date(2012, 11, 31),
                "Selecting a day from the previous month should select that date.");

            /*        December 2012                    December 2012
                 M   T   W   T   F   S   S        M   T   W   T   F   S   S
                26  27  28  29  30   1   2       26  27  28  29  30   1   2
                 3   4   5   6   7   8   9        3   4   5   6   7   8   9
                10  11  12  13  14  15  16  ==>  10  11  12  13  14  15  16
                17  18  19  20  21  22  23       17  18  19  20  21  22  23
                24  25  26  27  28  29  30       24  25  26  27  28  29 [30]
               [31]  1   2   3   4   5   6       31   1   2   3   4   5   6 */
            keyPress(Qt.Key_Left);
            compare(calendar.selectedDate, new Date(2012, 11, 30),
                "Pressing the left key on the left edge should select the date "
                + "1 row above on the right edge.");

            /*        December 2012                    December 2012
                 M   T   W   T   F   S   S        M   T   W   T   F   S   S
                26  27  28  29  30   1   2       26  27  28  29  30   1   2
                 3   4   5   6   7   8   9        3   4   5   6   7   8   9
                10  11  12  13  14  15  16  ==>  10  11  12  13  14  15  16
                17  18  19  20  21  22  23       17  18  19  20  21  22  23
                24  25  26  27  28  29 [30]      24  25  26  27  28  29  30
                31   1   2   3   4   5   6      [31]  1   2   3   4   5   6 */
            keyPress(Qt.Key_Right);
            compare(calendar.selectedDate, new Date(2012, 11, 31),
                "Pressing the right key on the right edge should select the date "
                + "1 row below on the left edge.");

            /*          April 2013                        March 2013
                 M   T   W   T   F   S   S        M   T   W   T   F   S   S
                [1]  2   3   4   5   6   7       25  26  27  28   1   2   3
                 8   9  10  11  12  13  14        4   5   6   7   8   9  10
                15  16  17  18  19  20  21  ==>  11  12  13  14  15  16  17
                22  23  24  25  26  27  28       18  19  20  21  22  23  24
                29  30  31   1   2   3   4       25  26  27  28  29  30 [31]
                 5   6   7   8   9  10  11        1   2   3   4   5   6   7 */
            calendar.selectedDate = new Date(2013, 3, 1);
            keyPress(Qt.Key_Left);
            compare(calendar.selectedDate, new Date(2013, 2, 31),
                "Pressing the left key on the left edge of the first row should "
                + "select the last date of the previous month.");

            /*         January 2014                     January 2014
                 M   T   W   T   F   S   S        M   T   W   T   F   S   S
                30  31  [1]  2   3   4   5       30  31   1   2   3   4   5
                 6   7   8   9  10  11  12        6   7   8   9  10  11  12
                13  14  15  16  17  18  19  ==>  13  14  15  16  17  18  19
                20  21  22  23  24  25  26       20  21  22  23  24  25  26
                27  28  29  30  31   1   2       27  28  29  30 [31]  1   2
                 3   4   5   6   7   8   9        3   4   5   6   7   8   9 */
            calendar.selectedDate = new Date(2014, 0, 1);
            keyPress(Qt.Key_End);
            compare(calendar.selectedDate, new Date(2014, 0, 31),
                "Pressing the end key should select the last date in the same month.");

            /*         January 2014                     January 2014
                 M   T   W   T   F   S   S        M   T   W   T   F   S   S
                30  31   1   2   3   4   5       30  31  [1]  2   3   4   5
                 6   7   8   9  10  11  12        6   7   8   9  10  11  12
                13  14  15  16  17  18  19  ==>  13  14  15  16  17  18  19
                20  21  22  23  24  25  26       20  21  22  23  24  25  26
                27  28  29  30 [31]  1   2       27  28  29  30  31   1   2
                 3   4   5   6   7   8   9        3   4   5   6   7   8   9 */
            calendar.selectedDate = new Date(2014, 0, 31);
            keyPress(Qt.Key_Home);
            compare(calendar.selectedDate, new Date(2014, 0, 1),
                "Pressing the home key should select the first date in the same month.");

            /*         December 2013                     November 2013
                 M   T   W   T   F   S   S        M   T   W   T   F   S   S
                25  26  27  28  29  30   1       28  29  30  31  [1]  2   3
                 2   3   4   5   6   7   8        4   5   6   7   8   9  10
                 9  10  11  12  13  14  15  ==>  11  12  13  14  15  16  17
                16  17  18  19  20  21  22       18  19  20  21  22  23  24
                23  24  25  26  27  28  29       25  26  27  28  29 [30]  1
                30 [31]  1   2   3   4   5        2   3   4   5   6   7   8 */
            calendar.selectedDate = new Date(2013, 11, 31);
            keyPress(Qt.Key_PageUp);
            compare(calendar.selectedDate, new Date(2013, 10, 30),
                "Pressing the page up key should select the equivalent date in the previous month.");

            /*          November 2013                   December 2013
                 M   T   W   T   F   S   S        M   T   W   T   F   S   S
                28  29  30  31  [1]  2   3       25  26  27  28  29  30   1
                 4   5   6   7   8   9  10        2   3   4   5   6   7   8
                11  12  13  14  15  16  17  ==>   9  10  11  12  13  14  15
                18  19  20  21  22  23  24       16  17  18  19  20  21  22
                25  26  27  28  29 [30]  1       23  24  25  26  27  28  29
                 2   3   4   5   6   7   8      [30]  31  1   2   3   4   5 */
            calendar.selectedDate = new Date(2013, 10, 30);
            keyPress(Qt.Key_PageDown);
            compare(calendar.selectedDate, new Date(2013, 11, 30),
                "Pressing the page down key should select the equivalent date in the next month.");
        }

        function test_selectPreviousMonth() {
            calendar.selectedDate = new Date(2013, 0, 1);
            compare(calendar.selectedDate, new Date(2013, 0, 1));

            calendar.__selectPreviousMonth();
            compare(calendar.selectedDate, new Date(2012, 11, 1));
        }

        function test_showPreviousMonthWithMouse() {
            calendar.selectedDate = new Date(2013, 1, 28);
            compare(calendar.visibleMonth, 1);
            compare(calendar.visibleYear, 2013);

            mouseClick(calendar, previousMonthButtonX, previousMonthButtonY, Qt.LeftButton);
            compare(calendar.visibleMonth, 0);
            compare(calendar.visibleYear, 2013);

            mouseClick(calendar, previousMonthButtonX, previousMonthButtonY, Qt.LeftButton);
            compare(calendar.visibleMonth, 11);
            compare(calendar.visibleYear, 2012);
        }

        function test_selectNextMonth() {
            calendar.selectedDate = new Date(2013, 0, 31);
            compare(calendar.selectedDate, new Date(2013, 0, 31));

            calendar.__selectNextMonth();
            compare(calendar.selectedDate, new Date(2013, 1, 28));

            calendar.__selectNextMonth();
            compare(calendar.selectedDate, new Date(2013, 2, 28));
        }

        function test_showNextMonthWithMouse() {
            calendar.selectedDate = new Date(2013, 0, 31);
            compare(calendar.visibleMonth, 0);
            compare(calendar.visibleYear, 2013);

            mouseClick(calendar, nextMonthButtonX, nextMonthButtonY, Qt.LeftButton);
            compare(calendar.visibleMonth, 1);
            compare(calendar.visibleYear, 2013);

            mouseClick(calendar, nextMonthButtonX, nextMonthButtonY, Qt.LeftButton);
            compare(calendar.visibleMonth, 2);
            compare(calendar.visibleYear, 2013);
        }

        function test_selectDateWithMouse() {
            /*         January 2013
                 S   M   T   W   T   F   S
                30  31  [1]  2   3   4   5
                 6   7   8   9  10  11  12
                13  14  15  16  17  18  19
                20  21  22  23  24  25  26
                27  28  29  30  31   1   2
                 3   4   5   6   7   8   9  */

            var startDate = new Date(2013, 0, 1);
            calendar.selectedDate = startDate;
            calendar.__locale = Qt.locale("en_US");
            compare(calendar.selectedDate, startDate);

            pressedSignalSpy.target = calendar;
            pressedSignalSpy.signalName = "pressed";

            clickedSignalSpy.target = calendar;
            clickedSignalSpy.signalName = "clicked";

            releasedSignalSpy.target = calendar;
            releasedSignalSpy.signalName = "released";

            // Clicking on header items should do nothing.
            mousePress(calendar, 0, navigationBarHeight, Qt.LeftButton);
            compare(calendar.selectedDate, startDate);
            compare(calendar.__panel.pressedCellIndex, -1);
            compare(pressedSignalSpy.count, 0);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);

            mouseRelease(calendar, 0, navigationBarHeight, Qt.LeftButton);
            compare(calendar.selectedDate, startDate);
            compare(calendar.__panel.pressedCellIndex, -1);
            compare(pressedSignalSpy.count, 0);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);

            var firstVisibleDate = new Date(2012, 11, 30);
            for (var week = 0; week < CalendarUtils.weeksOnACalendarMonth; ++week) {
                for (var day = 0; day < CalendarUtils.daysInAWeek; ++day) {
                    var expectedDate = new Date(firstVisibleDate);
                    var cellIndex = (week * CalendarUtils.daysInAWeek + day);
                    expectedDate.setDate(expectedDate.getDate() + cellIndex);

                    mousePress(calendar, toPixelsX(day), toPixelsY(week), Qt.LeftButton);
                    compare(calendar.selectedDate, expectedDate);
                    compare(calendar.__panel.pressedCellIndex, cellIndex);
                    compare(pressedSignalSpy.count, 1);
                    compare(releasedSignalSpy.count, 0);
                    compare(clickedSignalSpy.count, 0);

                    mouseRelease(calendar, toPixelsX(day), toPixelsY(week), Qt.LeftButton);
                    compare(calendar.__panel.pressedCellIndex, -1);
                    compare(pressedSignalSpy.count, 1);
                    compare(releasedSignalSpy.count, 1);
                    compare(clickedSignalSpy.count, 1);

                    pressedSignalSpy.clear();
                    releasedSignalSpy.clear();
                    clickedSignalSpy.clear();

                    if (expectedDate.getMonth() != startDate.getMonth()) {
                        // A different month is being displayed as a result of the click;
                        // change back to the original month so our results are correct.
                        calendar.selectedDate = startDate;
                        compare(calendar.selectedDate, startDate);
                    }
                }
            }

            // Ensure released event does not trigger date selection.
            calendar.selectedDate = startDate;
            mousePress(calendar, toPixelsX(1), toPixelsY(0), Qt.LeftButton);
            compare(calendar.selectedDate, new Date(2012, 11, 31));
            compare(calendar.__panel.pressedCellIndex, 1);
            compare(pressedSignalSpy.count, 1);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);

            pressedSignalSpy.clear();
            releasedSignalSpy.clear();
            clickedSignalSpy.clear();

            mouseRelease(calendar, toPixelsX(1), toPixelsY(0), Qt.LeftButton);
            compare(calendar.selectedDate, new Date(2012, 11, 31));
            compare(calendar.__panel.pressedCellIndex, -1);
            compare(pressedSignalSpy.count, 0);
            compare(releasedSignalSpy.count, 1);
            compare(clickedSignalSpy.count, 1);
        }

        function test_selectInvalidDateWithMouse() {
            var startDate = new Date(2013, 0, 1);
            calendar.minimumDate = new Date(2013, 0, 1);
            calendar.selectedDate = new Date(startDate);
            calendar.maximumDate = new Date(2013, 1, 5);
            calendar.__locale = Qt.locale("no_NO");

            pressedSignalSpy.target = calendar;
            pressedSignalSpy.signalName = "pressed";

            clickedSignalSpy.target = calendar;
            clickedSignalSpy.signalName = "clicked";

            releasedSignalSpy.target = calendar;
            releasedSignalSpy.signalName = "released";

            /*         January 2013
                 M   T   W   T   F   S   S
               [31]  1   2   3   4   5   6
                 7   8   9  10  11  12  13
                14  15  16  17  18  19  20
                21  22  23  24  25  26  27
                28  29  30  31   1   2   3
                 4   5   6   7   8   9  10 */
            mousePress(calendar, toPixelsX(0), toPixelsY(0), Qt.LeftButton);
            compare(calendar.__panel.pressedCellIndex, 0);
            compare(calendar.selectedDate, startDate);
            compare(pressedSignalSpy.count, 0);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);

            mouseRelease(calendar, toPixelsX(0), toPixelsY(0), Qt.LeftButton);
            compare(calendar.selectedDate, startDate);
            compare(calendar.__panel.pressedCellIndex, -1);
            compare(pressedSignalSpy.count, 0);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);

            /*         January 2013                        December 2012
                 M   T   W   T   F   S   S            M   T   W   T   F   S   S
                31   1   2   3   4   5   6           31   1   2   3   4   5   6
                 7   8   9  10  11  12  13            7   8   9  10  11  12  13
                14  15  16  17  18  19  20  through  14  15  16  17  18  19  20
                21  22  23  24  25  26  27     to    21  22  23  24  25  26  27
                28  29  30  31   1   2   3           28  29  30  31   1   2   3
                 4   5  [6]  7   8   9  10            4   5   6   7   8   9 [10] */
            for (var x = 2; x < 7; ++x) {
                mousePress(calendar, toPixelsX(x), toPixelsY(5), Qt.LeftButton);
                compare(calendar.selectedDate, startDate);
                compare(calendar.__panel.pressedCellIndex, 35 + x);
                compare(pressedSignalSpy.count, 0);
                compare(releasedSignalSpy.count, 0);
                compare(clickedSignalSpy.count, 0);

                mouseRelease(calendar, toPixelsX(x), toPixelsY(5), Qt.LeftButton);
                compare(calendar.selectedDate, startDate);
                compare(calendar.__panel.pressedCellIndex, -1);
                compare(pressedSignalSpy.count, 0);
                compare(releasedSignalSpy.count, 0);
                compare(clickedSignalSpy.count, 0);
            }
        }

        function test_daysCenteredVertically() {
            /*
                When the first day of the visible month is the first cell in
                the calendar, we want to push it onto the next row to ensure
                there is some balance between the days shown in the previous
                and next months.

                       December 2014
                 M   T   W   T   F   S   S
                24  25  26  27  28  29  30
                 1   2   3   4   5   6   7
                 8   9  10  11  12  13  14
                15  16  17  18  19  20  21
                22  23  24  25  26  27  28
                29  30  31   1   2   3   4 */

            calendar.__locale = Qt.locale("en_GB");
            calendar.selectedDate = new Date(2014, 11, 1);
            mousePress(calendar, toPixelsX(0), toPixelsY(0), Qt.LeftButton);
            compare(calendar.selectedDate, new Date(2014, 10, 24));
            mouseRelease(calendar, toPixelsX(0), toPixelsY(0), Qt.LeftButton);
        }

        function test_hovered() {
            // Moving the mouse within the calendar once seems to be necessary for it
            // to receive subsequent move events.
            mouseMove(calendar, calendar.width, calendar.height, 100);

            hoveredSignalSpy.target = calendar;
            hoveredSignalSpy.signalName = "hovered";

            var selectedDate = calendar.selectedDate;
            var visibleMonth = calendar.visibleMonth;
            var visibleYear = calendar.visibleYear;
            for (var row = 0; row < CalendarUtils.weeksOnACalendarMonth; ++row) {
                for (var col = 0; col < CalendarUtils.daysInAWeek; ++col) {
                    mouseMove(calendar, toPixelsX(col), toPixelsY(row));

                    compare(hoveredSignalSpy.count, 1);
                    compare(calendar.__panel.hoveredCellIndex, row * CalendarUtils.daysInAWeek + col);
                    compare(calendar.selectedDate, selectedDate);
                    compare(calendar.visibleMonth, visibleMonth);
                    compare(calendar.visibleYear, visibleYear);

                    hoveredSignalSpy.clear();
                }
            }

            // Moving the mouse around over the same cell should only emit one hovered signal.
            mouseMove(calendar, toPixelsX(0), toPixelsY(0));
            compare(hoveredSignalSpy.count, 1);

            mouseMove(calendar, toPixelsX(0) + 1, toPixelsY(0) + 1);
            compare(hoveredSignalSpy.count, 1);

            hoveredSignalSpy.clear();

            // Moving the mouse outside the control should unset any hovered data.
            mouseMove(calendar, -1, -1);
            compare(calendar.__panel.hoveredCellIndex, -1);
            compare(hoveredSignalSpy.count, 0);

            // Moving it back in should set the hovered data.
            mouseMove(calendar, toPixelsX(0), toPixelsY(0));
            compare(hoveredSignalSpy.count, 1);
            compare(calendar.__panel.hoveredCellIndex, 0);
        }

        function dragTo(cellX, cellY, expectedCellIndex, expectedDate) {
            mouseMove(calendar, toPixelsX(cellX), toPixelsY(cellY), Qt.LeftButton);
            compare(calendar.selectedDate, expectedDate);
            compare(calendar.__panel.pressedCellIndex, expectedCellIndex);
            compare(hoveredSignalSpy.count, 1);
            compare(pressedSignalSpy.count, 1);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);

            hoveredSignalSpy.clear();
            pressedSignalSpy.clear();
            releasedSignalSpy.clear();
            clickedSignalSpy.clear();
        }

        function test_dragWhileMousePressed() {
            calendar.minimumDate = new Date(2014, 1, 1);
            calendar.selectedDate = new Date(2014, 1, 28);
            calendar.maximumDate = new Date(2014, 2, 31);
            calendar.__locale = Qt.locale("en_GB");

            hoveredSignalSpy.target = calendar;
            hoveredSignalSpy.signalName = "hovered";

            pressedSignalSpy.target = calendar;
            pressedSignalSpy.signalName = "pressed";

            clickedSignalSpy.target = calendar;
            clickedSignalSpy.signalName = "clicked";

            releasedSignalSpy.target = calendar;
            releasedSignalSpy.signalName = "released";

            /*
                This test drags across each row, alternating from left to right
                when an edge is reached.

                       February 2014                        February 2014
                 M   T   W   T   F   S   S            M   T   W   T   F   S   S
                27  28  29  30  31  [1]  2           27  28  29  30  31   1   2
                 3   4   5   6   7   8   9            3   4   5   6   7   8   9
                10  11  12  13  14  15  16  through  10  11  12  13  14  15  16
                17  18  19  20  21  22  23     to    17  18  19  20  21  22  23
                24  25  26  27  28   1   2           24  25  26  27 [28]  1   2
                 3   4   5   6   7   8   9            3   4   5   6   7   8   9 */

            mousePress(calendar, toPixelsX(5), toPixelsY(0), Qt.LeftButton);
            compare(calendar.selectedDate, new Date(2014, 1, 1));
            compare(calendar.__panel.pressedCellIndex, 5);
            compare(hoveredSignalSpy.count, 1);
            compare(pressedSignalSpy.count, 1);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);

            hoveredSignalSpy.clear();
            pressedSignalSpy.clear();
            releasedSignalSpy.clear();
            clickedSignalSpy.clear();

            // The first row just has one drag.
            dragTo(6, 0, 6, new Date(2014, 1, 2));

            // Second row, right to left.
            for (var x = 6, index = 13; index >= 7; --x, --index)
                dragTo(x, Math.floor(index / CalendarUtils.daysInAWeek), index, new Date(2014, 1, 9 - (6 - x)));

            // Third row, left to right.
            for (x = 0, index = 14; index <= 20; ++x, ++index)
                dragTo(x, Math.floor(index / CalendarUtils.daysInAWeek), index, new Date(2014, 1, 10 + x));

            // Fourth row, right to left.
            for (x = 6, index = 27; index >= 21; --x, --index) {
                dragTo(x, Math.floor(index / CalendarUtils.daysInAWeek), index, new Date(2014, 1, 23 - (6 - x)));
            }

            // Fifth row, left to right. Stop at the last day of the month.
            for (x = 0, index = 28; index <= 32; ++x, ++index) {
                dragTo(x, Math.floor(index / CalendarUtils.daysInAWeek), index, new Date(2014, 1, 24 + x));
            }

            // Dragging into the next month shouldn't work, as it can lead to
            // unwanted month changes if moving within a bunch of "next month" cells.
            // We still emit the signals as usual, though.
            var oldDate = calendar.selectedDate;
            mouseMove(calendar, toPixelsX(5), toPixelsY(4), Qt.LeftButton);
            compare(calendar.selectedDate, oldDate);
            compare(calendar.__panel.pressedCellIndex, 32);
            compare(calendar.__panel.hoveredCellIndex, 33);
            compare(hoveredSignalSpy.count, 1);
            compare(pressedSignalSpy.count, 1);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);

            hoveredSignalSpy.clear();
            pressedSignalSpy.clear();
            releasedSignalSpy.clear();
            clickedSignalSpy.clear();

            // Finish the drag over the day in the next month.
            mouseRelease(calendar, toPixelsX(5), toPixelsY(4), Qt.LeftButton);
            compare(calendar.selectedDate, oldDate);
            compare(calendar.__panel.pressedCellIndex, -1);
            compare(hoveredSignalSpy.count, 0);
            compare(pressedSignalSpy.count, 0);
            compare(releasedSignalSpy.count, 1);
            compare(clickedSignalSpy.count, 0);

            hoveredSignalSpy.clear();
            pressedSignalSpy.clear();
            releasedSignalSpy.clear();
            clickedSignalSpy.clear();

            // Now try dragging into an invalid date.
            calendar.selectedDate = calendar.minimumDate;
            compare(calendar.visibleMonth, calendar.minimumDate.getMonth());

            mouseMove(calendar, toPixelsX(4), toPixelsY(0), Qt.LeftButton);
            compare(calendar.selectedDate, calendar.minimumDate);
            compare(calendar.__panel.pressedCellIndex, -1);
            compare(hoveredSignalSpy.count, 0);
            compare(pressedSignalSpy.count, 0);
            compare(releasedSignalSpy.count, 0);
            compare(clickedSignalSpy.count, 0);
        }

        function ensureNoGapsBetweenCells(columns, rows, availableWidth, availableHeight) {
            for (var row = 1; row < rows; ++row) {
                for (var col = 1; col < columns; ++col) {
                    var lastHorizontalRect = CalendarUtils.cellRectAt((row - 1) * columns + col - 1, columns, rows, availableWidth, availableHeight);
                    var thisHorizontalRect = CalendarUtils.cellRectAt((row - 1) * columns + col, columns, rows, availableWidth, availableHeight);
                    compare (lastHorizontalRect.x + lastHorizontalRect.width, thisHorizontalRect.x,
                        "Gaps between column " + (col - 1) + " and " + col + " in a grid of " + columns + " columns and " + rows + " rows, "
                        + "with an availableWidth of " + availableWidth + " and availableHeight of " + availableHeight);

                    var lastVerticalRect = CalendarUtils.cellRectAt((row - 1) * columns + col - 1, columns, rows, availableWidth, availableHeight);
                    var thisVerticalRect = CalendarUtils.cellRectAt(row * columns + col - 1, columns, rows, availableWidth, availableHeight);
                    compare (lastVerticalRect.y + lastVerticalRect.height, thisVerticalRect.y,
                        "Gaps between row " + (row - 1) + " and " + row + " in a grid of " + columns + " columns and " + rows + " rows, "
                        + "with an availableWidth of " + availableWidth + " and availableHeight of " + availableHeight);
                }
            }
        }

        function ensureGapsBetweenCells(columns, rows, availableWidth, availableHeight, gridLineWidth) {
            for (var row = 1; row < rows; ++row) {
                for (var col = 1; col < columns; ++col) {
                    var lastHorizontalRect = CalendarUtils.cellRectAt((row - 1) * columns + col - 1, columns, rows, availableWidth, availableHeight);
                    var thisHorizontalRect = CalendarUtils.cellRectAt((row - 1) * columns + col, columns, rows, availableWidth, availableHeight);
                    compare (lastHorizontalRect.x + lastHorizontalRect.width + gridLineWidth, thisHorizontalRect.x,
                        "No gap for grid line between column " + (col - 1) + " and " + col + " in a grid of " + columns + " columns and " + rows + " rows, "
                        + "with an availableWidth of " + availableWidth + " and availableHeight of " + availableHeight);

                    var lastVerticalRect = CalendarUtils.cellRectAt((row - 1) * columns + col - 1, columns, rows, availableWidth, availableHeight);
                    var thisVerticalRect = CalendarUtils.cellRectAt(row * columns + col - 1, columns, rows, availableWidth, availableHeight);
                    compare (lastVerticalRect.y + lastVerticalRect.height + gridLineWidth, thisVerticalRect.y,
                        "No gap for grid line between row " + (row - 1) + " and " + row + " in a grid of " + columns + " columns and " + rows + " rows, "
                        + "with an availableWidth of " + availableWidth + " and availableHeight of " + availableHeight);
                }
            }
        }

        function test_gridlessCellRectCalculation() {
            var columns = CalendarUtils.daysInAWeek;
            var rows = CalendarUtils.weeksOnACalendarMonth;
            var gridLineWidth = 0;

            // No extra space available.
            var availableWidth = 10 * columns;
            var availableHeight = 10 * rows;
            var rect = CalendarUtils.cellRectAt(0, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, 0);
            compare(rect.y, 0);
            compare(rect.width, 10);
            compare(rect.height, 10);

            rect = CalendarUtils.cellRectAt(columns - 1, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, (columns - 1) * 10);
            compare(rect.y, 0);
            compare(rect.width, 10);
            compare(rect.height, 10);

            rect = CalendarUtils.cellRectAt(rows * columns - 1, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, (columns - 1) * 10);
            compare(rect.y, (rows - 1) * 10);
            compare(rect.width, 10);
            compare(rect.height, 10);

            ensureNoGapsBetweenCells(columns, rows, availableWidth, availableHeight, gridLineWidth);

            // 1 extra pixel of space in both width and height.
            availableWidth = 10 * columns + 1;
            availableHeight = 10 * rows + 1;
            rect = CalendarUtils.cellRectAt(0, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, 0);
            compare(rect.y, 0);
            compare(rect.width, 10 + 1);
            compare(rect.height, 10 + 1);

            rect = CalendarUtils.cellRectAt(columns - 1, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, (columns - 1) * 10 + 1);
            compare(rect.y, 0);
            compare(rect.width, 10);
            compare(rect.height, 10 + 1);

            rect = CalendarUtils.cellRectAt(rows * columns - 1, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, (columns - 1) * 10 + 1);
            compare(rect.y, (rows - 1) * 10 + 1);
            compare(rect.width, 10);
            compare(rect.height, 10);

            ensureNoGapsBetweenCells(columns, rows, availableWidth, availableHeight);

            // 6 extra pixels in width, 5 in height.
            availableWidth = 10 * columns + 6;
            availableHeight = 10 * rows + 5;
            rect = CalendarUtils.cellRectAt(0, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, 0);
            compare(rect.y, 0);
            compare(rect.width, 10 + 1);
            compare(rect.height, 10 + 1);

            rect = CalendarUtils.cellRectAt(columns - 1, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, (columns - 1) * 10 + 6);
            compare(rect.y, 0);
            compare(rect.width, 10);
            compare(rect.height, 10 + 1);

            rect = CalendarUtils.cellRectAt(rows * columns - 1, columns, rows, availableWidth, availableHeight, gridLineWidth);
            compare(rect.x, (columns - 1) * 10 + 6);
            compare(rect.y, (rows - 1) * 10 + 5);
            compare(rect.width, 10);
            compare(rect.height, 10);

            ensureNoGapsBetweenCells(columns, rows, availableWidth, availableHeight);

            availableWidth = 280;
            availableHeight = 190;
            ensureNoGapsBetweenCells(columns, rows, availableWidth, availableHeight);

            for (var i = 0; i < columns; ++i) {
                ++availableWidth;
                ensureNoGapsBetweenCells(columns, rows, availableWidth, availableHeight, gridLineWidth);
            }

            for (i = 0; i < columns; ++i) {
                ++availableHeight;
                ensureNoGapsBetweenCells(columns, rows, availableWidth, availableHeight, gridLineWidth);
            }
        }

        function test_gridCellRectCalculation() {
            var columns = CalendarUtils.daysInAWeek;
            var rows = CalendarUtils.weeksOnACalendarMonth;
            var gridLineWidth = 1;

            var availableWidth = 10 * columns;
            var availableHeight = 10 * rows;
            var expectedXs = [0, 11, 21, 31, 41, 51, 61];
            var expectedWidths = [10, 9, 9, 9, 9, 9, 9];
            // Expected y positions and heights actually are the same as the x positions and widths in this case.
            // The code below assumes that columns >= rows; if this becomes false, arrays for the expected
            // y positions and heights must be created to avoid out of bounds accesses.
            for (var row = 0; row < rows; ++row) {
                for (var col = 0; col < columns; ++col) {
                    var index = row * columns + col;
                    var rect = CalendarUtils.cellRectAt(index, columns, rows, availableWidth, availableHeight, gridLineWidth);
                    compare(rect.x, expectedXs[col]);
                    compare(rect.y, expectedXs[row]);
                    compare(rect.width, expectedWidths[col]);
                    compare(rect.height, expectedWidths[row]);
                }
            }

            // The available width and height of a 250x250 calendar (its implicit size).
            availableWidth = 250;
            availableHeight = 168;
            expectedXs = [0, 36, 72, 108, 144, 180, 216];
            var expectedYs = [0, 29, 57, 85, 113, 141];
            expectedWidths = [35, 35, 35, 35, 35, 35, 34];
            var expectedHeights = [28, 27, 27, 27, 27, 27];
            for (row = 0; row < rows; ++row) {
                for (col = 0; col < columns; ++col) {
                    index = row * columns + col;
                    rect = CalendarUtils.cellRectAt(index, columns, rows, availableWidth, availableHeight, gridLineWidth);
                    compare(rect.x, expectedXs[col]);
                    compare(rect.y, expectedYs[row]);
                    compare(rect.width, expectedWidths[col]);
                    compare(rect.height, expectedHeights[row]);
                }
            }
        }

        function test_pressAndHold() {
            calendar.selectedDate = new Date(2013, 0, 1);
            calendar.__locale = Qt.locale("en_GB");

            pressedSignalSpy.target = calendar;
            pressedSignalSpy.signalName = "pressed";

            releasedSignalSpy.target = calendar;
            releasedSignalSpy.signalName = "released";

            pressAndHoldSignalSpy.target = calendar;
            pressAndHoldSignalSpy.signalName = "pressAndHold";

            /*         January 2013
                 M   T   W   T   F   S   S
                31   1   2   3   4   5   6
                 7   8   9  10  11  12 [13]
                14  15  16  17  18  19  20
                21  22  23  24  25  26  27
                28  29  30  31   1   2   3
                 4   5   6   7   8   9  10 */
            mousePress(calendar, toPixelsX(6), toPixelsY(1), Qt.LeftButton);
            compare(calendar.__panel.pressedCellIndex, 13);
            compare(pressedSignalSpy.count, 1);
            compare(releasedSignalSpy.count, 0);
            compare(pressAndHoldSignalSpy.count, 0);
            tryCompare(pressAndHoldSignalSpy, "count", 1);

            mouseRelease(calendar, toPixelsX(6), toPixelsY(1), Qt.LeftButton);
            compare(calendar.__panel.pressedCellIndex, -1);
            compare(pressedSignalSpy.count, 1);
            compare(releasedSignalSpy.count, 1);
            compare(pressAndHoldSignalSpy.count, 1);
        }
    }
}
