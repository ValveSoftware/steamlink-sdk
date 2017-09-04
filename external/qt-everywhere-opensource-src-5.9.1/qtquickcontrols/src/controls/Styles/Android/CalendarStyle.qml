/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtQuick.Window 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

CalendarStyle {
    gridColor: "transparent"
    __verticalSeparatorColor: "transparent"
    __horizontalSeparatorColor: AndroidStyle.colorValue(AndroidStyle.styleDef.calendarViewStyle.CalendarView_weekSeparatorLineColor)

    background: Rectangle {
        implicitWidth: Screen.width * 0.8
        implicitHeight: Screen.height * 0.8
        color: AndroidStyle.colorValue(AndroidStyle.styleDef.calendarViewStyle.defaultBackgroundColor)
    }

    navigationBar: Row {
        ToolButton {
            id: prevButton
            text: "\u25c0" // BLACK LEFT-POINTING TRIANGLE
            onClicked: control.showPreviousMonth()
        }
        LabelStyle {
            id: navigationBar
            text: styleData.title
            focused: control.activeFocus
            window_focused: control.Window.active
            styleDef: AndroidStyle.styleDef.calendarViewStyle
            width: parent.width - prevButton.width - nextButton.width
            height: parent.height
        }
        ToolButton {
            id: nextButton
            text: "\u25b6" // BLACK RIGHT-POINTING TRIANGLE
            onClicked: control.showNextMonth()
        }
    }

    dayOfWeekDelegate: Item {
        implicitWidth: dayOfWeek.implicitWidth * 2
        implicitHeight: dayOfWeek.implicitHeight * 2
        LabelStyle {
            id: dayOfWeek
            anchors.centerIn: parent
            text: control.__locale.dayName(styleData.dayOfWeek, control.dayOfWeekFormat)
            focused: control.activeFocus
            window_focused: control.Window.active
            styleDef: AndroidStyle.styleDef.calendarViewStyle.CalendarView_weekDayTextAppearance
        }
    }

    weekNumberDelegate: Item {
        implicitWidth: weekNumber.implicitWidth * 2
        implicitHeight: weekNumber.implicitHeight * 2
        LabelStyle {
            id: weekNumber
            anchors.centerIn: parent
            text: styleData.weekNumber
            focused: control.activeFocus
            window_focused: control.Window.active
            styleDef: AndroidStyle.styleDef.calendarViewStyle
            color: AndroidStyle.colorValue(styleDef.CalendarView_weekNumberColor)
        }
    }

    dayDelegate: Rectangle {
        readonly property int row: styleData.index / 7
        readonly property int selectedRow: control.__model.indexAt(control.selectedDate) / 7

        anchors.fill: parent
        anchors.rightMargin: -1
        color: styleData.selected || row !== selectedRow ? "transparent" :
                   AndroidStyle.colorValue(AndroidStyle.styleDef.calendarViewStyle.CalendarView_selectedWeekBackgroundColor)

        DrawableLoader {
            height: parent.height
            focused: control.activeFocus
            window_focused: control.Window.active
            active: styleData.selected
            styleDef: AndroidStyle.styleDef.calendarViewStyle.CalendarView_selectedDateVerticalBar
            width: 6 // UNSCALED_SELECTED_DATE_VERTICAL_BAR_WIDTH
        }

        DrawableLoader {
            height: parent.height
            focused: control.activeFocus
            window_focused: control.Window.active
            anchors.right: parent.right
            active: styleData.selected
            styleDef: AndroidStyle.styleDef.calendarViewStyle.CalendarView_selectedDateVerticalBar
            width: 6 // UNSCALED_SELECTED_DATE_VERTICAL_BAR_WIDTH
        }

        LabelStyle {
            anchors.centerIn: parent
            text: styleData.date.getDate()
            enabled: styleData.valid
            pressed: styleData.pressed
            selected: styleData.selected
            focused: control.activeFocus
            window_focused: control.Window.active
            styleDef: AndroidStyle.styleDef.calendarViewStyle.CalendarView_dateTextAppearance
            color: styleData.valid && styleData.visibleMonth ? AndroidStyle.colorValue(AndroidStyle.styleDef.calendarViewStyle.CalendarView_focusedMonthDateColor)
                                                             : AndroidStyle.colorValue(AndroidStyle.styleDef.calendarViewStyle.CalendarView_unfocusedMonthDateColor)
        }

        Rectangle {
            visible: row > 0
            width: parent.width
            height: 1 // UNSCALED_WEEK_SEPARATOR_LINE_WIDTH
            color: AndroidStyle.colorValue(AndroidStyle.styleDef.calendarViewStyle.CalendarView_weekSeparatorLineColor)
        }
    }
}
