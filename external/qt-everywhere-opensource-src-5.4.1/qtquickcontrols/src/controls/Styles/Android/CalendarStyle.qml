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
