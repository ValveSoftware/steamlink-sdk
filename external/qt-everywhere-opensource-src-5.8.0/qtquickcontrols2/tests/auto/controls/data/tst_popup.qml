/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

import QtQuick 2.4
import QtTest 1.0
import QtQuick.Controls 2.1
import QtQuick.Templates 2.1 as T

TestCase {
    id: testCase
    width: 400
    height: 400
    visible: true
    when: windowShown
    name: "Popup"

    ApplicationWindow {
        id: applicationWindow
        width: 480
        height: 360
    }

    Component {
        id: popupTemplate
        T.Popup { }
    }

    Component {
        id: popupControl
        Popup { }
    }

    Component {
        id: rect
        Rectangle { }
    }

    Component {
        id: signalSpy
        SignalSpy { }
    }

    function test_padding() {
        var control = popupTemplate.createObject(testCase)
        verify(control)

        var paddingSpy = signalSpy.createObject(testCase, {target: control, signalName: "paddingChanged"})
        verify(paddingSpy.valid)

        var topPaddingSpy = signalSpy.createObject(testCase, {target: control, signalName: "topPaddingChanged"})
        verify(topPaddingSpy.valid)

        var leftPaddingSpy = signalSpy.createObject(testCase, {target: control, signalName: "leftPaddingChanged"})
        verify(leftPaddingSpy.valid)

        var rightPaddingSpy = signalSpy.createObject(testCase, {target: control, signalName: "rightPaddingChanged"})
        verify(rightPaddingSpy.valid)

        var bottomPaddingSpy = signalSpy.createObject(testCase, {target: control, signalName: "bottomPaddingChanged"})
        verify(bottomPaddingSpy.valid)

        var paddingChanges = 0
        var topPaddingChanges = 0
        var leftPaddingChanges = 0
        var rightPaddingChanges = 0
        var bottomPaddingChanges = 0

        compare(control.padding, 0)
        compare(control.topPadding, 0)
        compare(control.leftPadding, 0)
        compare(control.rightPadding, 0)
        compare(control.bottomPadding, 0)
        compare(control.availableWidth, 0)
        compare(control.availableHeight, 0)

        control.width = 100
        control.height = 100

        control.padding = 10
        compare(control.padding, 10)
        compare(control.topPadding, 10)
        compare(control.leftPadding, 10)
        compare(control.rightPadding, 10)
        compare(control.bottomPadding, 10)
        compare(paddingSpy.count, ++paddingChanges)
        compare(topPaddingSpy.count, ++topPaddingChanges)
        compare(leftPaddingSpy.count, ++leftPaddingChanges)
        compare(rightPaddingSpy.count, ++rightPaddingChanges)
        compare(bottomPaddingSpy.count, ++bottomPaddingChanges)

        control.topPadding = 20
        compare(control.padding, 10)
        compare(control.topPadding, 20)
        compare(control.leftPadding, 10)
        compare(control.rightPadding, 10)
        compare(control.bottomPadding, 10)
        compare(paddingSpy.count, paddingChanges)
        compare(topPaddingSpy.count, ++topPaddingChanges)
        compare(leftPaddingSpy.count, leftPaddingChanges)
        compare(rightPaddingSpy.count, rightPaddingChanges)
        compare(bottomPaddingSpy.count, bottomPaddingChanges)

        control.leftPadding = 30
        compare(control.padding, 10)
        compare(control.topPadding, 20)
        compare(control.leftPadding, 30)
        compare(control.rightPadding, 10)
        compare(control.bottomPadding, 10)
        compare(paddingSpy.count, paddingChanges)
        compare(topPaddingSpy.count, topPaddingChanges)
        compare(leftPaddingSpy.count, ++leftPaddingChanges)
        compare(rightPaddingSpy.count, rightPaddingChanges)
        compare(bottomPaddingSpy.count, bottomPaddingChanges)

        control.rightPadding = 40
        compare(control.padding, 10)
        compare(control.topPadding, 20)
        compare(control.leftPadding, 30)
        compare(control.rightPadding, 40)
        compare(control.bottomPadding, 10)
        compare(paddingSpy.count, paddingChanges)
        compare(topPaddingSpy.count, topPaddingChanges)
        compare(leftPaddingSpy.count, leftPaddingChanges)
        compare(rightPaddingSpy.count, ++rightPaddingChanges)
        compare(bottomPaddingSpy.count, bottomPaddingChanges)

        control.bottomPadding = 50
        compare(control.padding, 10)
        compare(control.topPadding, 20)
        compare(control.leftPadding, 30)
        compare(control.rightPadding, 40)
        compare(control.bottomPadding, 50)
        compare(paddingSpy.count, paddingChanges)
        compare(topPaddingSpy.count, topPaddingChanges)
        compare(leftPaddingSpy.count, leftPaddingChanges)
        compare(rightPaddingSpy.count, rightPaddingChanges)
        compare(bottomPaddingSpy.count, ++bottomPaddingChanges)

        control.padding = 60
        compare(control.padding, 60)
        compare(control.topPadding, 20)
        compare(control.leftPadding, 30)
        compare(control.rightPadding, 40)
        compare(control.bottomPadding, 50)
        compare(paddingSpy.count, ++paddingChanges)
        compare(topPaddingSpy.count, topPaddingChanges)
        compare(leftPaddingSpy.count, leftPaddingChanges)
        compare(rightPaddingSpy.count, rightPaddingChanges)
        compare(bottomPaddingSpy.count, bottomPaddingChanges)

        control.destroy()
    }

    function test_availableSize() {
        var control = popupTemplate.createObject(testCase)
        verify(control)

        var availableWidthSpy = signalSpy.createObject(testCase, {target: control, signalName: "availableWidthChanged"})
        verify(availableWidthSpy.valid)

        var availableHeightSpy = signalSpy.createObject(testCase, {target: control, signalName: "availableHeightChanged"})
        verify(availableHeightSpy.valid)

        var availableWidthChanges = 0
        var availableHeightChanges = 0

        control.width = 100
        compare(control.availableWidth, 100)
        compare(availableWidthSpy.count, ++availableWidthChanges)
        compare(availableHeightSpy.count, availableHeightChanges)

        control.height = 100
        compare(control.availableHeight, 100)
        compare(availableWidthSpy.count, availableWidthChanges)
        compare(availableHeightSpy.count, ++availableHeightChanges)

        control.padding = 10
        compare(control.availableWidth, 80)
        compare(control.availableHeight, 80)
        compare(availableWidthSpy.count, ++availableWidthChanges)
        compare(availableHeightSpy.count, ++availableHeightChanges)

        control.topPadding = 20
        compare(control.availableWidth, 80)
        compare(control.availableHeight, 70)
        compare(availableWidthSpy.count, availableWidthChanges)
        compare(availableHeightSpy.count, ++availableHeightChanges)

        control.leftPadding = 30
        compare(control.availableWidth, 60)
        compare(control.availableHeight, 70)
        compare(availableWidthSpy.count, ++availableWidthChanges)
        compare(availableHeightSpy.count, availableHeightChanges)

        control.rightPadding = 40
        compare(control.availableWidth, 30)
        compare(control.availableHeight, 70)
        compare(availableWidthSpy.count, ++availableWidthChanges)
        compare(availableHeightSpy.count, availableHeightChanges)

        control.bottomPadding = 50
        compare(control.availableWidth, 30)
        compare(control.availableHeight, 30)
        compare(availableWidthSpy.count, availableWidthChanges)
        compare(availableHeightSpy.count, ++availableHeightChanges)

        control.padding = 60
        compare(control.availableWidth, 30)
        compare(control.availableHeight, 30)
        compare(availableWidthSpy.count, availableWidthChanges)
        compare(availableHeightSpy.count, availableHeightChanges)

        control.width = 0
        compare(control.availableWidth, 0)
        compare(availableWidthSpy.count, ++availableWidthChanges)
        compare(availableHeightSpy.count, availableHeightChanges)

        control.height = 0
        compare(control.availableHeight, 0)
        compare(availableWidthSpy.count, availableWidthChanges)
        compare(availableHeightSpy.count, ++availableHeightChanges)

        control.destroy()
    }

    function test_position() {
        var control = popupControl.createObject(testCase, {visible: true, leftMargin: 10, topMargin: 20, width: 100, height: 100})
        verify(control)
        verify(control.visible)

        var xSpy = signalSpy.createObject(testCase, {target: control, signalName: "xChanged"})
        verify(xSpy.valid)

        var ySpy = signalSpy.createObject(testCase, {target: control, signalName: "yChanged"})
        verify(ySpy.valid)

        // moving outside margins does not trigger change notifiers
        control.x = -100
        compare(control.x, 10)
        compare(control.y, 20)
        compare(xSpy.count, 0)
        compare(ySpy.count, 0)

        control.y = -200
        compare(control.x, 10)
        compare(control.y, 20)
        compare(xSpy.count, 0)
        compare(ySpy.count, 0)

        // moving within margins triggers change notifiers
        control.x = 30
        compare(control.x, 30)
        compare(control.y, 20)
        compare(xSpy.count, 1)
        compare(ySpy.count, 0)

        control.y = 40
        compare(control.x, 30)
        compare(control.y, 40)
        compare(xSpy.count, 1)
        compare(ySpy.count, 1)

        // re-parent and reset the position
        control.parent = rect.createObject(testCase, {color: "red", width: 100, height: 100})
        control.x = 0
        control.y = 0
        compare(xSpy.count, 2)
        compare(ySpy.count, 2)

        // moving parent outside margins triggers change notifiers
        control.parent.x = -50
        compare(control.x, 50 + control.leftMargin)
        compare(xSpy.count, 3)
        compare(ySpy.count, 2)

        control.parent.y = -60
        compare(control.y, 60 + control.topMargin)
        compare(xSpy.count, 3)
        compare(ySpy.count, 3)

        control.destroy()
    }

    function test_resetSize() {
        var control = popupControl.createObject(testCase, {visible: true, margins: 0})
        verify(control)

        control.width = control.implicitWidth = testCase.width + 10
        control.height = control.implicitHeight = testCase.height + 10

        compare(control.width, testCase.width + 10)
        compare(control.height, testCase.height + 10)

        control.width = undefined
        control.height = undefined
        compare(control.width, testCase.width)
        compare(control.height, testCase.height)

        control.destroy()
    }

    function test_negativeMargins() {
        var control = popupControl.createObject(testCase, {implicitWidth: testCase.width, implicitHeight: testCase.height})
        verify(control)

        control.open()
        verify(control.visible)

        compare(control.x, 0)
        compare(control.y, 0)

        compare(control.margins, -1)
        compare(control.topMargin, -1)
        compare(control.leftMargin, -1)
        compare(control.rightMargin, -1)
        compare(control.bottomMargin, -1)

        control.x = -10
        control.y = -10
        compare(control.x, 0)
        compare(control.y, 0)

        control.destroy()
    }

    function test_margins() {
        var control = popupControl.createObject(testCase, {width: 100, height: 100})
        verify(control)

        control.open()
        verify(control.visible)

        control.margins = 10
        compare(control.margins, 10)
        compare(control.topMargin, 10)
        compare(control.leftMargin, 10)
        compare(control.rightMargin, 10)
        compare(control.bottomMargin, 10)
        compare(control.contentItem.parent.x, 10)
        compare(control.contentItem.parent.y, 10)

        control.topMargin = 20
        compare(control.margins, 10)
        compare(control.topMargin, 20)
        compare(control.leftMargin, 10)
        compare(control.rightMargin, 10)
        compare(control.bottomMargin, 10)
        compare(control.contentItem.parent.x, 10)
        compare(control.contentItem.parent.y, 20)

        control.leftMargin = 20
        compare(control.margins, 10)
        compare(control.topMargin, 20)
        compare(control.leftMargin, 20)
        compare(control.rightMargin, 10)
        compare(control.bottomMargin, 10)
        compare(control.contentItem.parent.x, 20)
        compare(control.contentItem.parent.y, 20)

        control.x = testCase.width
        control.y = testCase.height
        compare(control.contentItem.parent.x, testCase.width - control.width - 10)
        compare(control.contentItem.parent.y, testCase.height - control.height - 10)

        control.rightMargin = 20
        compare(control.margins, 10)
        compare(control.topMargin, 20)
        compare(control.leftMargin, 20)
        compare(control.rightMargin, 20)
        compare(control.bottomMargin, 10)
        compare(control.contentItem.parent.x, testCase.width - control.width - 20)
        compare(control.contentItem.parent.y, testCase.height - control.height - 10)

        control.bottomMargin = 20
        compare(control.margins, 10)
        compare(control.topMargin, 20)
        compare(control.leftMargin, 20)
        compare(control.rightMargin, 20)
        compare(control.bottomMargin, 20)
        compare(control.contentItem.parent.x, testCase.width - control.width - 20)
        compare(control.contentItem.parent.y, testCase.height - control.height - 20)

        control.margins = undefined
        compare(control.margins, -1)

        control.bottomMargin = undefined
        compare(control.bottomMargin, -1)
        compare(control.contentItem.parent.x, testCase.width - control.width - 20)
        compare(control.contentItem.parent.y, testCase.height)

        control.rightMargin = undefined
        compare(control.rightMargin, -1)
        compare(control.contentItem.parent.x, testCase.width)
        compare(control.contentItem.parent.y, testCase.height)

        control.x = -testCase.width
        control.y = -testCase.height
        compare(control.contentItem.parent.x, 20)
        compare(control.contentItem.parent.y, 20)

        control.topMargin = undefined
        compare(control.topMargin, -1)
        compare(control.contentItem.parent.x, 20)
        compare(control.contentItem.parent.y, -testCase.height)

        control.leftMargin = undefined
        compare(control.leftMargin, -1)
        compare(control.contentItem.parent.x, -testCase.width)
        compare(control.contentItem.parent.y, -testCase.height)

        control.destroy()
    }

    function test_background() {
        var control = popupTemplate.createObject(testCase)
        verify(control)

        control.background = rect.createObject(testCase)

        // background has no x or width set, so its width follows control's width
        control.width = 320
        compare(control.background.width, control.width)

        // background has no y or height set, so its height follows control's height
        compare(control.background.height, control.height)
        control.height = 240

        // has width => width does not follow
        control.background.width /= 2
        control.width += 20
        verify(control.background.width !== control.width)

        // reset width => width follows again
        control.background.width = undefined
        control.width += 20
        compare(control.background.width, control.width)

        // has x => width does not follow
        control.background.x = 10
        control.width += 20
        verify(control.background.width !== control.width)

        // has height => height does not follow
        control.background.height /= 2
        control.height -= 20
        verify(control.background.height !== control.height)

        // reset height => height follows again
        control.background.height = undefined
        control.height -= 20
        compare(control.background.height, control.height)

        // has y => height does not follow
        control.background.y = 10
        control.height -= 20
        verify(control.background.height !== control.height)

        control.destroy()
    }

    function getChild(control, objname, idx) {
        var index = idx
        for (var i = index+1; i < control.children.length; i++)
        {
            if (control.children[i].objectName === objname) {
                index = i
                break
            }
        }
        return index
    }

    Component {
        id: component
        ApplicationWindow {
            id: _window
            width: 400
            height: 400
            visible: true
            font.pixelSize: 40
            property alias pane: _pane
            property alias popup: _popup
            property SignalSpy fontspy: SignalSpy { target: _window; signalName: "fontChanged" }
            Pane {
                id: _pane
                property alias button: _button
                font.pixelSize: 30
                property SignalSpy fontspy: SignalSpy { target: _pane; signalName: "fontChanged" }
                Column {
                    Button {
                        id: _button
                        text: "Button"
                        font.pixelSize: 20
                        property SignalSpy fontspy: SignalSpy { target: _button; signalName: "fontChanged" }
                        Popup {
                            id: _popup
                            property alias button: _button2
                            property alias listview: _listview
                            y: _button.height
                            implicitHeight: Math.min(396, _listview.contentHeight)
                            property SignalSpy fontspy: SignalSpy { target: _popup; signalName: "fontChanged" }
                            contentItem: Column {
                                Button {
                                    id: _button2
                                    text: "Button"
                                    property SignalSpy fontspy: SignalSpy { target: _button2; signalName: "fontChanged" }
                                }
                                ListView {
                                    id: _listview
                                    height: _button.height * 20
                                    model: 2
                                    delegate: Button {
                                        id: _button3
                                        objectName: "delegate"
                                        width: _button.width
                                        height: _button.height
                                        text: "N: " + index
                                        checkable: true
                                        autoExclusive: true
                                        property SignalSpy fontspy: SignalSpy { target: _button3; signalName: "fontChanged" }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function test_font() { // QTBUG_50984, QTBUG-51696
        var window = component.createObject(testCase)
        verify(window)

        compare(window.font.pixelSize, 40)
        compare(window.pane.font.pixelSize, 30)
        compare(window.pane.button.font.pixelSize, 20)
        compare(window.popup.font.pixelSize, 40)
        compare(window.popup.button.font.pixelSize, 40)

        var idx1 = getChild(window.popup.listview.contentItem, "delegate", -1)
        compare(window.popup.listview.contentItem.children[idx1].font.pixelSize, 40)
        var idx2 = getChild(window.popup.listview.contentItem, "delegate", idx1)
        compare(window.popup.listview.contentItem.children[idx2].font.pixelSize, 40)

        window.pane.button.font.pixelSize = 30
        compare(window.font.pixelSize, 40)
        compare(window.fontspy.count, 0)
        compare(window.pane.font.pixelSize, 30)
        compare(window.pane.fontspy.count, 0)
        compare(window.pane.button.font.pixelSize, 30)
        compare(window.pane.button.fontspy.count, 1)
        compare(window.popup.font.pixelSize, 40)
        compare(window.popup.fontspy.count, 0)
        compare(window.popup.button.font.pixelSize, 40)
        compare(window.popup.button.fontspy.count, 0)
        compare(window.popup.listview.contentItem.children[idx1].font.pixelSize, 40)
        compare(window.popup.listview.contentItem.children[idx1].fontspy.count, 0)
        compare(window.popup.listview.contentItem.children[idx2].font.pixelSize, 40)
        compare(window.popup.listview.contentItem.children[idx2].fontspy.count, 0)

        window.font.pixelSize = 50
        compare(window.font.pixelSize, 50)
        compare(window.fontspy.count, 1)
        compare(window.pane.font.pixelSize, 30)
        compare(window.pane.fontspy.count, 0)
        compare(window.pane.button.font.pixelSize, 30)
        compare(window.pane.button.fontspy.count, 1)
        compare(window.popup.font.pixelSize, 50)
        compare(window.popup.fontspy.count, 1)
        compare(window.popup.button.font.pixelSize, 50)
        compare(window.popup.button.fontspy.count, 1)
        compare(window.popup.listview.contentItem.children[idx1].font.pixelSize, 50)
        compare(window.popup.listview.contentItem.children[idx1].fontspy.count, 1)
        compare(window.popup.listview.contentItem.children[idx2].font.pixelSize, 50)
        compare(window.popup.listview.contentItem.children[idx2].fontspy.count, 1)

        window.popup.button.font.pixelSize = 10
        compare(window.font.pixelSize, 50)
        compare(window.fontspy.count, 1)
        compare(window.pane.font.pixelSize, 30)
        compare(window.pane.fontspy.count, 0)
        compare(window.pane.button.font.pixelSize, 30)
        compare(window.pane.button.fontspy.count, 1)
        compare(window.popup.font.pixelSize, 50)
        compare(window.popup.fontspy.count, 1)
        compare(window.popup.button.font.pixelSize, 10)
        compare(window.popup.button.fontspy.count, 2)
        compare(window.popup.listview.contentItem.children[idx1].font.pixelSize, 50)
        compare(window.popup.listview.contentItem.children[idx1].fontspy.count, 1)
        compare(window.popup.listview.contentItem.children[idx2].font.pixelSize, 50)
        compare(window.popup.listview.contentItem.children[idx2].fontspy.count, 1)

        window.popup.font.pixelSize = 60
        compare(window.font.pixelSize, 50)
        compare(window.fontspy.count, 1)
        compare(window.pane.font.pixelSize, 30)
        compare(window.pane.fontspy.count, 0)
        compare(window.pane.button.font.pixelSize, 30)
        compare(window.pane.button.fontspy.count, 1)
        compare(window.popup.font.pixelSize, 60)
        compare(window.popup.fontspy.count, 2)
        compare(window.popup.button.font.pixelSize, 10)
        compare(window.popup.button.fontspy.count, 2)
        compare(window.popup.listview.contentItem.children[idx1].font.pixelSize, 60)
        compare(window.popup.listview.contentItem.children[idx1].fontspy.count, 2)
        compare(window.popup.listview.contentItem.children[idx2].font.pixelSize, 60)
        compare(window.popup.listview.contentItem.children[idx2].fontspy.count, 2)

        window.destroy()
    }

    Component {
        id: localeComponent
        Pane {
            property alias button: _button
            property alias popup: _popup
            locale: Qt.locale("en_US")
            Column {
                Button {
                    id: _button
                    text: "Button"
                    locale: Qt.locale("nb_NO")
                    Popup {
                        id: _popup
                        property alias button1: _button1
                        property alias button2: _button2
                        y: _button.height
                        locale: Qt.locale("fi_FI")
                        implicitHeight: Math.min(396, _column.contentHeight)
                        contentItem: Column {
                            id: _column
                            Button {
                                id: _button1
                                text: "Button 1"
                                objectName: "1"
                            }
                            Button {
                                id: _button2
                                text: "Button 2"
                                locale: Qt.locale("nb_NO")
                                objectName: "2"
                            }
                        }
                    }
                }
            }
        }
    }

    function test_locale() { // QTBUG_50984
        // test looking up natural locale from ancestors
        var control = localeComponent.createObject(applicationWindow.contentItem)
        verify(control)

        compare(control.locale.name, "en_US")
        compare(control.button.locale.name, "nb_NO")
        compare(control.popup.locale.name, "fi_FI")
        compare(control.popup.button1.locale.name, "fi_FI")
        compare(control.popup.button2.locale.name, "nb_NO")

        control.ApplicationWindow.window.locale = undefined
        control.destroy()
    }

    Component {
        id: localeChangeComponent
        Pane {
            id: _pane
            property alias button: _button
            property alias popup: _popup
            property SignalSpy localespy: SignalSpy {
                target: _pane
                signalName: "localeChanged"
            }
            property SignalSpy mirrorspy: SignalSpy {
                target: _pane
                signalName: "mirroredChanged"
            }
            Column {
                Button {
                    id: _button
                    text: "Button"
                    property SignalSpy localespy: SignalSpy {
                        target: _button
                        signalName: "localeChanged"
                    }
                    property SignalSpy mirrorspy: SignalSpy {
                        target: _button
                        signalName: "mirroredChanged"
                    }
                    Popup {
                        id: _popup
                        property alias button1: _button1
                        property alias button2: _button2
                        y: _button.height
                        implicitHeight: Math.min(396, _column.contentHeight)
                        property SignalSpy localespy: SignalSpy {
                            target: _popup
                            signalName: "localeChanged"
                        }
                        contentItem: Column {
                            id: _column
                            Button {
                                id: _button1
                                text: "Button 1"
                                property SignalSpy localespy: SignalSpy {
                                    target: _button1
                                    signalName: "localeChanged"
                                }
                                property SignalSpy mirrorspy: SignalSpy {
                                    target: _button1
                                    signalName: "mirroredChanged"
                                }
                            }
                            Button {
                                id: _button2
                                text: "Button 2"
                                property SignalSpy localespy: SignalSpy {
                                    target: _button2
                                    signalName: "localeChanged"
                                }
                                property SignalSpy mirrorspy: SignalSpy {
                                    target: _button2
                                    signalName: "mirroredChanged"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function test_locale_changes() { // QTBUG_50984
        // test default locale and locale inheritance
        var control = localeChangeComponent.createObject(applicationWindow.contentItem)
        verify(control)

        var defaultLocale = Qt.locale()
        compare(control.ApplicationWindow.window.locale.name, defaultLocale.name)
        compare(control.locale.name, defaultLocale.name)
        compare(control.button.locale.name, defaultLocale.name)
        compare(control.popup.locale.name, defaultLocale.name)
        compare(control.popup.button1.locale.name, defaultLocale.name)
        compare(control.popup.button2.locale.name, defaultLocale.name)

        control.ApplicationWindow.window.locale = Qt.locale("nb_NO")
        compare(control.ApplicationWindow.window.locale.name, "nb_NO")
        compare(control.locale.name, "nb_NO")
        compare(control.button.locale.name, "nb_NO")
        compare(control.popup.locale.name, "nb_NO")
        compare(control.popup.button1.locale.name, "nb_NO")
        compare(control.popup.button2.locale.name, "nb_NO")
        compare(control.localespy.count, 1)
        compare(control.button.localespy.count, 1)
        compare(control.popup.localespy.count, 1)
        compare(control.popup.button1.localespy.count, 1)
        compare(control.popup.button2.localespy.count, 1)

        control.ApplicationWindow.window.locale = undefined
        compare(control.ApplicationWindow.window.locale.name, defaultLocale.name)
        compare(control.locale.name, defaultLocale.name)
        compare(control.button.locale.name, defaultLocale.name)
        compare(control.popup.locale.name, defaultLocale.name)
        compare(control.popup.button1.locale.name, defaultLocale.name)
        compare(control.popup.button2.locale.name, defaultLocale.name)
        compare(control.localespy.count, 2)
        compare(control.button.localespy.count, 2)
        compare(control.popup.localespy.count, 2)
        compare(control.popup.button1.localespy.count, 2)
        compare(control.popup.button2.localespy.count, 2)

        control.locale = Qt.locale("ar_EG")
        compare(control.ApplicationWindow.window.locale.name, defaultLocale.name)
        compare(control.locale.name, "ar_EG")
        compare(control.button.locale.name, "ar_EG")
        compare(control.popup.locale.name, defaultLocale.name)
        compare(control.popup.button1.locale.name, defaultLocale.name)
        compare(control.popup.button2.locale.name, defaultLocale.name)
        compare(control.localespy.count, 3)
        compare(control.mirrorspy.count, 1)
        compare(control.button.localespy.count, 3)
        compare(control.button.mirrorspy.count, 1)
        compare(control.popup.localespy.count, 2)
        compare(control.popup.button1.localespy.count, 2)
        compare(control.popup.button2.localespy.count, 2)

        control.ApplicationWindow.window.locale = Qt.locale("ar_EG")
        compare(control.ApplicationWindow.window.locale.name, "ar_EG")
        compare(control.locale.name, "ar_EG")
        compare(control.button.locale.name, "ar_EG")
        compare(control.popup.locale.name, "ar_EG")
        compare(control.popup.button1.locale.name, "ar_EG")
        compare(control.popup.button2.locale.name, "ar_EG")
        compare(control.localespy.count, 3)
        compare(control.mirrorspy.count, 1)
        compare(control.button.localespy.count, 3)
        compare(control.button.mirrorspy.count, 1)
        compare(control.popup.localespy.count, 3)
        compare(control.popup.button1.localespy.count, 3)
        compare(control.popup.button1.mirrorspy.count, 1)
        compare(control.popup.button2.localespy.count, 3)
        compare(control.popup.button2.mirrorspy.count, 1)

        control.button.locale = Qt.locale("nb_NO")
        compare(control.ApplicationWindow.window.locale.name, "ar_EG")
        compare(control.locale.name, "ar_EG")
        compare(control.button.locale.name, "nb_NO")
        compare(control.popup.locale.name, "ar_EG")
        compare(control.popup.button1.locale.name, "ar_EG")
        compare(control.popup.button2.locale.name, "ar_EG")
        compare(control.localespy.count, 3)
        compare(control.mirrorspy.count, 1)
        compare(control.button.localespy.count, 4)
        compare(control.button.mirrorspy.count, 2)
        compare(control.popup.localespy.count, 3)
        compare(control.popup.button1.localespy.count, 3)
        compare(control.popup.button2.localespy.count, 3)

        control.locale = undefined
        compare(control.ApplicationWindow.window.locale.name, "ar_EG")
        compare(control.locale.name, "ar_EG")
        compare(control.button.locale.name, "nb_NO")
        compare(control.popup.locale.name, "ar_EG")
        compare(control.popup.button1.locale.name, "ar_EG")
        compare(control.popup.button2.locale.name, "ar_EG")
        compare(control.localespy.count, 3)
        compare(control.mirrorspy.count, 1)
        compare(control.button.localespy.count, 4)
        compare(control.button.mirrorspy.count, 2)
        compare(control.popup.localespy.count, 3)
        compare(control.popup.button1.localespy.count, 3)
        compare(control.popup.button2.localespy.count, 3)

        control.popup.button1.locale = Qt.locale("nb_NO")
        compare(control.ApplicationWindow.window.locale.name, "ar_EG")
        compare(control.locale.name, "ar_EG")
        compare(control.button.locale.name, "nb_NO")
        compare(control.popup.locale.name, "ar_EG")
        compare(control.popup.button1.locale.name, "nb_NO")
        compare(control.popup.button2.locale.name, "ar_EG")
        compare(control.localespy.count, 3)
        compare(control.mirrorspy.count, 1)
        compare(control.button.localespy.count, 4)
        compare(control.button.mirrorspy.count, 2)
        compare(control.popup.localespy.count, 3)
        compare(control.popup.button1.localespy.count, 4)
        compare(control.popup.button1.mirrorspy.count, 2)
        compare(control.popup.button2.localespy.count, 3)
        compare(control.popup.button2.mirrorspy.count, 1)

        control.popup.locale = Qt.locale("fi_FI")
        compare(control.ApplicationWindow.window.locale.name, "ar_EG")
        compare(control.locale.name, "ar_EG")
        compare(control.button.locale.name, "nb_NO")
        compare(control.popup.locale.name, "fi_FI")
        compare(control.popup.button1.locale.name, "nb_NO")
        compare(control.popup.button2.locale.name, "fi_FI")
        compare(control.localespy.count, 3)
        compare(control.mirrorspy.count, 1)
        compare(control.button.localespy.count, 4)
        compare(control.button.mirrorspy.count, 2)
        compare(control.popup.localespy.count, 4)
        compare(control.popup.button1.localespy.count, 4)
        compare(control.popup.button1.mirrorspy.count, 2)
        compare(control.popup.button2.localespy.count, 4)
        compare(control.popup.button2.mirrorspy.count, 2)

        control.ApplicationWindow.window.locale = undefined
        compare(control.ApplicationWindow.window.locale.name, defaultLocale.name)
        compare(control.locale.name, defaultLocale.name)
        compare(control.button.locale.name, "nb_NO")
        compare(control.popup.locale.name, "fi_FI")
        compare(control.popup.button1.locale.name, "nb_NO")
        compare(control.popup.button2.locale.name, "fi_FI")
        compare(control.localespy.count, 4)
        compare(control.mirrorspy.count, 2)
        compare(control.button.localespy.count, 4)
        compare(control.button.mirrorspy.count, 2)
        compare(control.popup.localespy.count, 4)
        compare(control.popup.button1.localespy.count, 4)
        compare(control.popup.button1.mirrorspy.count, 2)
        compare(control.popup.button2.localespy.count, 4)
        compare(control.popup.button2.mirrorspy.count, 2)

        control.popup.locale = undefined
        compare(control.ApplicationWindow.window.locale.name, defaultLocale.name)
        compare(control.locale.name, defaultLocale.name)
        compare(control.button.locale.name, "nb_NO")
        compare(control.popup.locale.name, defaultLocale.name)
        compare(control.popup.button1.locale.name, "nb_NO")
        compare(control.popup.button2.locale.name, defaultLocale.name)
        compare(control.localespy.count, 4)
        compare(control.mirrorspy.count, 2)
        compare(control.button.localespy.count, 4)
        compare(control.button.mirrorspy.count, 2)
        compare(control.popup.localespy.count, 5)
        compare(control.popup.button1.localespy.count, 4)
        compare(control.popup.button1.mirrorspy.count, 2)
        compare(control.popup.button2.localespy.count, 5)
        compare(control.popup.button2.mirrorspy.count, 2)

        control.destroy()
    }

    function test_size() {
        var control = popupControl.createObject(testCase)
        verify(control)

        control.open()
        waitForRendering(control.contentItem)

        // implicit size of the content
        control.contentItem.implicitWidth = 10
        compare(control.implicitWidth, 10 + control.leftPadding + control.rightPadding)
        compare(control.width, control.implicitWidth)
        compare(control.contentItem.width, control.width - control.leftPadding - control.rightPadding)

        control.contentItem.implicitHeight = 20
        compare(control.implicitHeight, 20 + control.topPadding + control.bottomPadding)
        compare(control.height, control.implicitHeight)
        compare(control.contentItem.height, control.height - control.topPadding - control.bottomPadding)

        // implicit size of the popup
        control.implicitWidth = 30
        compare(control.implicitWidth, 30)
        compare(control.width, 30)
        compare(control.contentItem.width, control.width - control.leftPadding - control.rightPadding)

        control.implicitHeight = 40
        compare(control.implicitHeight, 40)
        compare(control.height, 40)
        compare(control.contentItem.height, control.height - control.topPadding - control.bottomPadding)

        // set explicit size
        control.width = 50
        compare(control.implicitWidth, 30)
        compare(control.width, 50)
        compare(control.contentItem.width, control.width - control.leftPadding - control.rightPadding)

        control.height = 60
        compare(control.implicitHeight, 40)
        compare(control.height, 60)
        compare(control.contentItem.height, control.height - control.topPadding - control.bottomPadding)

        // reset explicit size
        control.width = undefined
        compare(control.implicitWidth, 30)
        compare(control.width, 30)
        compare(control.contentItem.width, control.width - control.leftPadding - control.rightPadding)

        control.height = undefined
        compare(control.implicitHeight, 40)
        compare(control.height, 40)
        compare(control.contentItem.height, control.height - control.topPadding - control.bottomPadding)

        control.destroy()
    }

    function test_visible() {
        var control = popupTemplate.createObject(testCase, {visible: true})
        verify(control)

        // QTBUG-51989
        tryCompare(control, "visible", true)

        // QTBUG-55347
        control.parent = null
        verify(!control.visible)

        control.destroy()
    }

    Component {
        id: overlayTest
        ApplicationWindow {
            property alias firstDrawer: firstDrawer
            property alias secondDrawer: secondDrawer
            property alias modalPopup: modalPopup
            property alias modelessPopup: modelessPopup
            property alias plainPopup: plainPopup
            property alias modalPopupWithoutDim: modalPopupWithoutDim
            visible: true
            Drawer {
                z: 0
                id: firstDrawer
            }
            Drawer {
                z: 1
                id: secondDrawer
            }
            Popup {
                id: modalPopup
                z: 2
                modal: true
                exit: Transition { PauseAnimation { duration: 200 } }
            }
            Popup {
                id: modelessPopup
                z: 3
                dim: true
                exit: Transition { PauseAnimation { duration: 200 } }
            }
            Popup {
                id: plainPopup
                z: 4
                enter: Transition { PauseAnimation { duration: 200 } }
                exit: Transition { PauseAnimation { duration: 200 } }
            }
            Popup {
                id: modalPopupWithoutDim
                z: 5
                dim: false
                modal: true
                exit: Transition { PauseAnimation { duration: 200 } }
            }
        }
    }

    function indexOf(array, item) {
        for (var idx = 0; idx < array.length; ++idx) {
            if (item === array[idx])
                return idx;
        }
        return -1
    }

    function findOverlay(window, popup) {
        var item = popup.contentItem.parent
        var idx = indexOf(window.overlay.children, item)
        return window.overlay.children[idx - 1]
    }

    function test_overlay() {
        var window = overlayTest.createObject(testCase)
        verify(window)

        window.requestActivate()
        tryCompare(window, "active", true)

        compare(window.overlay.children.length, 0)

        var firstOverlay = findOverlay(window, window.firstDrawer)
        verify(!firstOverlay)
        window.firstDrawer.open()
        compare(window.overlay.children.length, 2) // 1 drawer + 1 overlay
        firstOverlay = findOverlay(window, window.firstDrawer)
        verify(firstOverlay)
        compare(firstOverlay.z, window.firstDrawer.z)
        compare(indexOf(window.overlay.children, firstOverlay),
                indexOf(window.overlay.children, window.firstDrawer.contentItem.parent) - 1)
        tryCompare(firstOverlay, "opacity", 1.0)

        var secondOverlay = findOverlay(window, window.secondDrawer)
        verify(!secondOverlay)
        window.secondDrawer.open()
        compare(window.overlay.children.length, 4) // 2 drawers + 2 overlays
        secondOverlay = findOverlay(window, window.secondDrawer)
        verify(secondOverlay)
        compare(secondOverlay.z, window.secondDrawer.z)
        compare(indexOf(window.overlay.children, secondOverlay),
                indexOf(window.overlay.children, window.secondDrawer.contentItem.parent) - 1)
        tryCompare(secondOverlay, "opacity", 1.0)

        window.firstDrawer.close()
        tryCompare(window.firstDrawer, "visible", false)
        firstOverlay = findOverlay(window, window.firstDrawer)
        verify(!firstOverlay)
        compare(window.overlay.children.length, 2) // 1 drawer + 1 overlay

        window.secondDrawer.close()
        tryCompare(window.secondDrawer, "visible", false)
        secondOverlay = findOverlay(window, window.secondDrawer)
        verify(!secondOverlay)
        compare(window.overlay.children.length, 0)

        var modalOverlay = findOverlay(window, window.modalPopup)
        verify(!modalOverlay)
        window.modalPopup.open()
        modalOverlay = findOverlay(window, window.modalPopup)
        verify(modalOverlay)
        compare(modalOverlay.z, window.modalPopup.z)
        compare(window.modalPopup.visible, true)
        tryCompare(modalOverlay, "opacity", 1.0)
        compare(window.overlay.children.length, 2) // 1 popup + 1 overlay

        var modelessOverlay = findOverlay(window, window.modelessPopup)
        verify(!modelessOverlay)
        window.modelessPopup.open()
        modelessOverlay = findOverlay(window, window.modelessPopup)
        verify(modelessOverlay)
        compare(modelessOverlay.z, window.modelessPopup.z)
        compare(window.modelessPopup.visible, true)
        tryCompare(modelessOverlay, "opacity", 1.0)
        compare(window.overlay.children.length, 4) // 2 popups + 2 overlays

        window.modelessPopup.close()
        tryCompare(window.modelessPopup, "visible", false)
        modelessOverlay = findOverlay(window, window.modelessPopup)
        verify(!modelessOverlay)
        compare(window.overlay.children.length, 2) // 1 popup + 1 overlay

        compare(window.modalPopup.visible, true)
        compare(modalOverlay.opacity, 1.0)

        window.modalPopup.close()
        tryCompare(window.modalPopup, "visible", false)
        modalOverlay = findOverlay(window, window.modalPopup)
        verify(!modalOverlay)
        compare(window.overlay.children.length, 0)

        window.plainPopup.open()
        tryCompare(window.plainPopup, "visible", true)
        compare(window.overlay.children.length, 1) // only popup added, no overlays involved

        window.plainPopup.modal = true
        compare(window.overlay.children.length, 2) // overlay added

        window.plainPopup.close()
        tryCompare(window.plainPopup, "visible", false)
        compare(window.overlay.children.length, 0) // popup + overlay removed

        window.modalPopupWithoutDim.open()
        tryCompare(window.modalPopupWithoutDim, "visible", true)
        compare(window.overlay.children.length, 1) // only popup added, no overlays involved

        window.modalPopupWithoutDim.dim = true
        compare(window.overlay.children.length, 2) // overlay added

        window.modalPopupWithoutDim.close()
        tryCompare(window.modalPopupWithoutDim, "visible", false)
        compare(window.overlay.children.length, 0) // popup + overlay removed

        window.destroy()
    }

    function test_attached_applicationwindow() {
        var control = popupControl.createObject(applicationWindow.contentItem)
        verify(control)

        var child = rect.createObject(control.contentItem)

        compare(control.ApplicationWindow.window, applicationWindow)
        compare(control.contentItem.ApplicationWindow.window, applicationWindow)
        compare(child.ApplicationWindow.window, applicationWindow)

        control.parent = null
        compare(control.ApplicationWindow.window, null)
        compare(control.contentItem.ApplicationWindow.window, null)
        compare(child.ApplicationWindow.window, null)

        control.destroy()
    }

    SignalSpy {
        id: openedSpy
        signalName: "opened"
    }

    SignalSpy {
        id: closedSpy
        signalName: "closed"
    }

    Component {
        id: pausePopup
        Popup {
            enter: Transition { PauseAnimation { duration: 200 } }
            exit: Transition { PauseAnimation { duration: 200 } }
        }
    }

    function test_openedClosed() {
        var control = pausePopup.createObject(testCase)
        verify(control)

        openedSpy.target = control
        closedSpy.target = control

        control.open()
        compare(control.visible, true)
        compare(openedSpy.count, 0)
        tryCompare(openedSpy, "count", 1)
        compare(closedSpy.count, 0)

        control.close()
        compare(openedSpy.count, 1)
        compare(closedSpy.count, 0)
        tryCompare(closedSpy, "count", 1)
        compare(control.visible, false)

        control.destroy()
    }

    Component {
        id: xyBindingLoop
        ApplicationWindow {
            id: window
            width: 360
            height: 360
            visible: true
            property alias popup: popup

            Popup {
                id: popup
                visible: true
                x: (parent.width - width) / 2
                y: (parent.height - height) / 2
                Label {
                    text: "Content"
                    anchors.fill: parent
                }
            }
        }
    }

    function test_xyBindingLoop() {
        var window = xyBindingLoop.createObject(testCase)
        var control = window.popup
        waitForRendering(control.contentItem)
        compare(control.x, (control.parent.width - control.width) / 2)
        compare(control.y, (control.parent.height - control.height) / 2)
        window.destroy()
    }
}
