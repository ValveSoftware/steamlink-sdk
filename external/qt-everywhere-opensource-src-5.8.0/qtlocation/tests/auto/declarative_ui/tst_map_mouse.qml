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

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.6
import QtPositioning 5.5

    /*
     MouseArea setup for this test case.
     Map dimensions are 100 * 100
     Item containing map is 120,120

                                         (50,50)
     (0,0)   ---------------------------------------------------- (100,0)
             | no mouse area            | mouse area overlapper |
             |                          |                       |
     (0,20)  ---------------------------------------------------- (100,20)
             | mouse area upper         | mouse area upper,     |
             |                          | mouse area overlapper |
             |                          |                       |
             |                          |                       |
             |                          |                       |
     (0,50)  ---------------------------------------------------- (100,50)
             | mouse area lower         | mouse area lower,     |
             |                          | mouse area overlapper |
             |                          |                       |
             |                          |                       |
             |                          |                       |
             |                          |                       |
             |                          |                       |
             |                          |                       |
     (0,100) ---------------------------------------------------- (100,100)    |
                                                                               |
                                                                               |
                                                                      ----------(120, 120)

     */

Item {
    id: page
    x: 0; y: 0;
    width: 120
    height: 120
    // General-purpose elements for the test:
    Plugin { id: testPlugin; name : "qmlgeo.test.plugin"; allowExperimental: true }

    function setMouseData(ma, me)
    {
        ma.lastX = me.x
        ma.lastY = me.y
        ma.lastButton = me.button
        ma.lastButtons = me.buttons
        ma.lastModifiers = me.modifiers
        ma.lastWasHeld = me.wasHeld
        ma.lastIsClick = me.isClick
        ma.lastAccepted = me.accepted
    }

    Map {
        id: map;
        x: 0; y: 0; width: 100; height: 100
        center {
            latitude: 20
            longitude: 20
        }

        plugin: testPlugin;

        MouseArea {
            id: mouseUpper
            objectName: "mouseUpper"
            x: 0; y: 20; width: 100; height: 29
            property int lastX: -1
            property int lastY: -1
            property int lastButton: Qt.NoButton
            property int lastButtons: Qt.NoButton
            property int lastModifiers: Qt.NoModifier
            property bool lastWasHeld: false;
            property bool lastIsClick: false
            property bool lastAccepted: false;

            preventStealing: true

            // The following signal handlers use arguments[0] instead of mouse due to QTBUG-36560
            onClicked: page.setMouseData(mouseUpper, arguments[0])
            onDoubleClicked: page.setMouseData(mouseUpper, arguments[0])
            onPressed: page.setMouseData(mouseUpper, arguments[0])
            onReleased: page.setMouseData(mouseUpper, arguments[0])
            onPositionChanged: page.setMouseData(mouseUpper, arguments[0])
            onPressAndHold: page.setMouseData(mouseUpper, arguments[0])
        }
        MouseArea {
            id: mouseLower
            objectName: "mouseLower"
            x: 0; y: 50; width: 100; height: 50
            property int lastX: -1
            property int lastY: -1
            property int lastButton: Qt.NoButton
            property int lastButtons: Qt.NoButton
            property int lastModifiers: Qt.NoModifier
            property bool lastWasHeld: false;
            property bool lastIsClick: false
            property bool lastAccepted: false;

            // The following signal handlers use arguments[0] instead of mouse due to QTBUG-36560
            onClicked: page.setMouseData(mouseLower, arguments[0])
            onDoubleClicked: page.setMouseData(mouseLower, arguments[0])
            onPressed: page.setMouseData(mouseLower, arguments[0])
            onReleased: page.setMouseData(mouseLower, arguments[0])
            onPositionChanged: page.setMouseData(mouseLower, arguments[0])
            onPressAndHold: page.setMouseData(mouseLower, arguments[0])
        }
        MouseArea {
            id: mouseOverlapper
            objectName: "mouseOverlapper"
            x: 50; y: 0; width: 50; height: 100
            property int lastX: -1
            property int lastY: -1
            property int lastButton: Qt.NoButton
            property int lastButtons: Qt.NoButton
            property int lastModifiers: Qt.NoModifier
            property bool lastWasHeld: false;
            property bool lastIsClick: false
            property bool lastAccepted: false;

            // The following signal handlers use arguments[0] instead of mouse due to QTBUG-36560
            onClicked: page.setMouseData(mouseOverlapper, arguments[0])
            onDoubleClicked: page.setMouseData(mouseOverlapper, arguments[0])
            onPressed: page.setMouseData(mouseOverlapper, arguments[0])
            onReleased: page.setMouseData(mouseOverlapper, arguments[0])
            onPositionChanged: page.setMouseData(mouseOverlapper, arguments[0])
            onPressAndHold: page.setMouseData(mouseOverlapper, arguments[0])
        }
    }

    TestCase {
        name: "MouseArea"
        when: windowShown
        SignalSpy {id: mouseUpperClickedSpy; target: mouseUpper; signalName: "clicked"}
        SignalSpy {id: mouseLowerClickedSpy; target: mouseLower; signalName: "clicked"}
        SignalSpy {id: mouseOverlapperClickedSpy; target: mouseOverlapper; signalName: "clicked"}
        SignalSpy {id: mouseUpperDoubleClickedSpy; target: mouseUpper; signalName: "doubleClicked"}
        SignalSpy {id: mouseLowerDoubleClickedSpy; target: mouseLower; signalName: "doubleClicked"}
        SignalSpy {id: mouseOverlapperDoubleClickedSpy; target: mouseOverlapper; signalName: "doubleClicked"}
        SignalSpy {id: mouseUpperPressedSpy; target: mouseUpper; signalName: "onPressed"}
        SignalSpy {id: mouseLowerPressedSpy; target: mouseLower; signalName: "onPressed"}
        SignalSpy {id: mouseOverlapperPressedSpy; target: mouseOverlapper; signalName: "onPressed"}
        SignalSpy {id: mouseUpperReleasedSpy; target: mouseUpper; signalName: "released"}
        SignalSpy {id: mouseLowerReleasedSpy; target: mouseLower; signalName: "released"}
        SignalSpy {id: mouseOverlapperReleasedSpy; target: mouseOverlapper; signalName: "released"}
        SignalSpy {id: mouseUpperPositionChangedSpy; target: mouseUpper; signalName: "positionChanged"}
        SignalSpy {id: mouseLowerPositionChangedSpy; target: mouseLower; signalName: "positionChanged"}
        SignalSpy {id: mouseOverlapperPositionChangedSpy; target: mouseOverlapper; signalName: "positionChanged"}
        SignalSpy {id: mouseUpperPressAndHoldSpy; target: mouseUpper; signalName: "pressAndHold"}
        SignalSpy {id: mouseLowerPressAndHoldSpy; target: mouseLower; signalName: "pressAndHold"}
        SignalSpy {id: mouseOverlapperPressAndHoldSpy; target: mouseOverlapper; signalName: "pressAndHold"}
        SignalSpy {id: mouseUpperEnteredSpy; target: mouseUpper; signalName: "entered"}
        SignalSpy {id: mouseLowerEnteredSpy; target: mouseLower; signalName: "entered"}
        SignalSpy {id: mouseOverlapperEnteredSpy; target: mouseOverlapper; signalName: "entered"}
        SignalSpy {id: mouseUpperExitedSpy; target: mouseUpper; signalName: "exited"}
        SignalSpy {id: mouseLowerExitedSpy; target: mouseLower; signalName: "exited"}
        SignalSpy {id: mouseOverlapperExitedSpy; target: mouseOverlapper; signalName: "exited"}

        SignalSpy {id: mouseUpperEnabledChangedSpy; target: mouseUpper; signalName: "enabledChanged"}
        SignalSpy {id: mouseUpperAcceptedButtonsChangedSpy; target: mouseUpper; signalName: "acceptedButtonsChanged"}
        SignalSpy {id: mouseUpperPressedButtonsChangedSpy; target: mouseUpper; signalName: "pressedChanged"}
        SignalSpy {id: mouseUpperHoveredChangedSpy; target: mouseUpper; signalName: "hoveredChanged"}
        SignalSpy {id: mouseUpperPressedChangedSpy; target: mouseUpper; signalName: "pressedChanged"}

        SignalSpy {id: mouseOverlapperEnabledChangedSpy; target: mouseOverlapper; signalName: "enabledChanged"}

        function clear_data() {
            mouseUpperClickedSpy.clear()
            mouseLowerClickedSpy.clear()
            mouseOverlapperClickedSpy.clear()
            mouseUpperDoubleClickedSpy.clear()
            mouseLowerDoubleClickedSpy.clear()
            mouseOverlapperDoubleClickedSpy.clear()
            mouseUpperPressedSpy.clear()
            mouseLowerPressedSpy.clear()
            mouseOverlapperPressedSpy.clear()
            mouseUpperReleasedSpy.clear()
            mouseLowerReleasedSpy.clear()
            mouseOverlapperReleasedSpy.clear()
            mouseUpperPositionChangedSpy.clear()
            mouseLowerPositionChangedSpy.clear()
            mouseOverlapperPositionChangedSpy.clear()
            mouseUpperPressAndHoldSpy.clear()
            mouseLowerPressAndHoldSpy.clear()
            mouseOverlapperPressAndHoldSpy.clear()
            mouseUpperEnteredSpy.clear()
            mouseLowerEnteredSpy.clear()
            mouseOverlapperEnteredSpy.clear()
            mouseUpperExitedSpy.clear()
            mouseLowerExitedSpy.clear()
            mouseOverlapperExitedSpy.clear()

            mouseUpperEnabledChangedSpy.clear()
            mouseUpperAcceptedButtonsChangedSpy.clear()
            mouseUpperPressedButtonsChangedSpy.clear()
            mouseUpperHoveredChangedSpy.clear()
            mouseUpperPressedChangedSpy.clear()
            mouseUpperPositionChangedSpy.clear()

            mouseOverlapperEnabledChangedSpy.clear()
        }
        // these 'real_' prefixed functions do sequences as
        // it would occur on real app (e.g. doubleclick is in fact
        // a sequence of press, release, doubleclick, release).
        // (they were recorded as seen on test app). mouseClick() works ok
        // because testlib internally converts it to mousePress + mouseRelease events
        function real_click (target, x, y) {
            mousePress(target, x,y)
            mouseRelease(target, x, y)
        }
        function real_double_click (target, x, y) {
            mousePress(target, x,y)
            mouseRelease(target, x, y)
            mousePress(target, x, y)
            mouseDoubleClick(target, x, y)
            mouseRelease(target, x, y)
        }
        function real_press_and_hold(target, x,y) {
            mousePress(target,x,y)
            wait(1000) // threshold is 800 ms
            mouseRelease(target,x, y)
        }

        function test_enabled() {
            clear_data()
            // check that disabling overlapping mouse areas let events flow through
            mouseUpper.enabled = false
            compare(mouseUpperEnabledChangedSpy.count, 1)
            compare(mouseUpperClickedSpy.count, 0)
            mouseClick(map, 5, 25)
            compare(mouseUpperClickedSpy.count, 0)
            mouseUpper.enabled = true
            mouseClick(map, 5, 25)
            tryCompare(mouseUpperClickedSpy, "count", 1)
            compare(mouseUpperEnabledChangedSpy.count, 2)
            // when overlapping are is disabled, the event should flow through
            compare(mouseOverlapperClickedSpy.count, 0)
            mouseClick(map, 55, 25)
            tryCompare(mouseUpperClickedSpy, "count", 1)
            compare(mouseOverlapperClickedSpy.count, 1)
            mouseOverlapper.enabled = false
            compare(mouseOverlapperEnabledChangedSpy.count, 1)
            compare(mouseOverlapper.enabled, false)
            mouseClick(map, 55, 25)
            tryCompare(mouseOverlapperClickedSpy, "count", 1)
            compare(mouseUpperClickedSpy.count, 2)
            // re-enable and verify that still works
            mouseOverlapper.enabled = true
            compare(mouseOverlapperEnabledChangedSpy.count, 2)
            compare(mouseOverlapper.enabled, true)
            mouseClick(map, 55, 25)
            tryCompare(mouseOverlapperClickedSpy, "count", 2) // should consume again
            compare(mouseUpperClickedSpy.count, 2)
        }

        function test_wheel() {
            clear_data()
            wait(500);
            // on map but without mouse area
            var startZoomLevel = 6.20
            map.zoomLevel = startZoomLevel
            mouseWheel(map, 5, 5, 15, 5, Qt.LeftButton, Qt.NoModifiers)
            //see QDeclarativeGeoMapGestureArea::handleWheelEvent
            var endZoomLevel = startZoomLevel + 5 * 0.001
            compare(map.zoomLevel,endZoomLevel)

            map.zoomLevel = startZoomLevel
            mouseWheel(map, 5, 5, -15, -5, Qt.LeftButton, Qt.NoModifiers)
            //see QDeclarativeGeoMapGestureArea::handleWheelEvent
            endZoomLevel = startZoomLevel - 5 * 0.001
            compare(map.zoomLevel,endZoomLevel)

            // on map on top of mouse area
            map.zoomLevel = startZoomLevel
            mouseWheel(map, 55, 75, -30, -2, Qt.LeftButton, Qt.NoModifiers)
            endZoomLevel = startZoomLevel - 2 * 0.001
            compare(map.zoomLevel,endZoomLevel)

            // outside of map
            map.zoomLevel = startZoomLevel
            mouseWheel(map, -100, -100, 40, 4, Qt.LeftButton, Qt.NoModifiers)
            compare(map.zoomLevel,startZoomLevel)
        }

        function test_aaa_basic_properties() // _aaa_ to ensure execution first
        {
            clear_data()
            wait(50)
            // default values
            compare(mouseUpper.containsMouse, false)
            compare(mouseUpper.pressed, false)
            compare(mouseUpper.enabled, true)
            compare(mouseUpper.pressedButtons, 0)
            compare(mouseUpper.acceptedButtons, Qt.LeftButton)
            // accepted buttons
            compare(mouseUpperAcceptedButtonsChangedSpy.count, 0)
            mouseUpper.acceptedButtons = Qt.RightButton  | Qt.MiddleButton
            compare(mouseUpper.acceptedButtons, Qt.RightButton | Qt.MiddleButton)
            compare(mouseUpperAcceptedButtonsChangedSpy.count, 1)
            mouseClick(map, 5, 25)
            compare(mouseUpperClickedSpy.count, 0) // left button not accepted
            mouseUpper.acceptedButtons = Qt.LeftButton
            compare(mouseUpperAcceptedButtonsChangedSpy.count, 2)
            mouseClick(map, 5, 25)
            tryCompare(mouseUpperClickedSpy, "count", 1)
        }

        function test_basic_position_changed() {
            // tests basic position changed/move when button is being pressed
            clear_data();
            wait(500);
            mousePress(map, 5, 25)
            compare(mouseUpperPressedSpy.count, 1)
            compare(mouseUpper.lastAccepted, true)
            compare(mouseUpper.lastButton, Qt.LeftButton)
            compare(mouseUpper.lastButtons, Qt.LeftButton)
            compare(mouseUpper.lastModifiers, Qt.NoModifier)
            // moves within the mouse area
            mouseMove(map, 5, 26, 0, Qt.LeftButton) // '0' is 'delay'
            wait(1) // mouseMove event goes one extra eventloop round in the test lib
            compare(mouseUpperEnteredSpy.count, 1)
            compare(mouseUpperPositionChangedSpy.count, 1)
            compare(mouseUpper.mouseX, 5)
            compare(mouseUpper.mouseY, 6) // 20 offset, mouseXY is relative to the mouse area
            compare(mouseUpper.lastAccepted, true)
            compare(mouseUpper.lastButton, Qt.NoButton)
            compare(mouseUpper.lastButtons, Qt.LeftButton) // buttons being pressed whilst movin'
            compare(mouseUpper.lastModifiers, Qt.NoModifier)
            compare(mouseUpper.lastWasHeld, false) // testfunction won't take required 800 ms
            compare(mouseUpper.lastX, 5)
            compare(mouseUpper.lastY, 6) // remember 20 offset of the mouse area

            mouseMove(map, 6, 27, 0, Qt.LeftButton | Qt.RightButton)
            wait(1)
            compare(mouseUpperEnteredSpy.count, 1) // no re-entry
            compare(mouseUpperPositionChangedSpy.count, 2)
            compare(mouseUpper.mouseX, 6)
            compare(mouseUpper.mouseY, 7)
            compare(mouseUpper.lastAccepted, true)
            compare(mouseUpper.lastButton, Qt.NoButton)
            compare(mouseUpper.lastButtons, Qt.LeftButton | Qt.RightButton) // buttons being pressed whilst movin'
            compare(mouseUpper.lastModifiers, Qt.NoModifier)
            compare(mouseUpper.lastWasHeld, false) // testfunction won't take required 800 ms
            compare(mouseUpper.lastX, 6)
            compare(mouseUpper.lastY, 7) // remember 20 offset of the mouse area

            // moves outside of mouse but within map
            mouseMove(map, 2, 2, 0)
            wait(1)
            compare(mouseUpperExitedSpy.count, 1)
            compare(mouseUpperPositionChangedSpy.count, 3)
            compare(mouseUpper.mouseX, 2)
            compare(mouseUpper.mouseY, -18)
            // come back to map
            mouseMove(map, 7, 28, 0)
            wait(1)
            compare(mouseUpperEnteredSpy.count, 2)
            compare(mouseUpperExitedSpy.count, 1)
            compare(mouseUpperPositionChangedSpy.count, 4)
            compare(mouseUpper.mouseX, 7)
            compare(mouseUpper.mouseY, 8)

            // move outside of widget area (left). make sure that other mouse areas won't get the events
            mouseMove(map, -10, 10, 0)
            wait(1)
            compare(mouseUpperPositionChangedSpy.count, 5)
            compare(mouseUpperExitedSpy.count, 2)
            compare(mouseUpper.mouseX, -10)
            compare(mouseUpper.mouseY, -10)

            // back in and then on top of the widget
            mouseMove(map, 5, 25, 0)
            wait(1)
            compare(mouseUpperPositionChangedSpy.count, 6)
            compare(mouseUpperExitedSpy.count, 2)
            compare(mouseUpperEnteredSpy.count, 3)
            compare(mouseUpper.mouseX, 5)
            compare(mouseUpper.mouseY, 5)
            mouseMove(map, 5, -25, 0)
            wait(1)
            compare(mouseUpperPositionChangedSpy.count, 7)
            compare(mouseUpperExitedSpy.count, 3)
            compare(mouseUpperEnteredSpy.count, 3)
            compare(mouseUpper.mouseX, 5)
            compare(mouseUpper.mouseY, -45)

            // back in then float on top of other mouse areas
            mouseMove(map, 5, 25, 0)
            wait(1)
            compare(mouseUpperPositionChangedSpy.count, 8)
            compare(mouseUpperExitedSpy.count, 3)
            compare(mouseUpperEnteredSpy.count, 4)
            compare(mouseUpper.mouseX, 5)
            compare(mouseUpper.mouseY, 5)
            mouseMove(map, 5, 75, 0)
            wait(1)
            compare(mouseUpperPositionChangedSpy.count, 9)
            compare(mouseUpperExitedSpy.count, 4)
            compare(mouseUpperEnteredSpy.count, 4)
            compare(mouseUpper.mouseX, 5)
            compare(mouseUpper.mouseY, 55) // remember the 20 offset of upper mouse area
            mouseMove(map, 75, 75, 0)
            wait(1)
            compare(mouseUpperPositionChangedSpy.count, 10)
            compare(mouseUpperExitedSpy.count, 4)
            compare(mouseUpperEnteredSpy.count, 4)
            compare(mouseUpper.mouseX, 75)
            compare(mouseUpper.mouseY, 55)
            // finally back in
            mouseMove(map, 5, 25, 0)
            wait(1)
            compare(mouseUpperPositionChangedSpy.count, 11)
            compare(mouseUpperExitedSpy.count, 4)
            compare(mouseUpperEnteredSpy.count, 5)
            compare(mouseUpper.mouseX, 5)
            compare(mouseUpper.mouseY, 5)

            // check that these fellas didn't get any stupid ideas
            compare(mouseLowerEnteredSpy.count, 0)
            compare(mouseLowerPositionChangedSpy.count, 0)
            compare(mouseOverlapperEnteredSpy.count, 0)
            compare(mouseOverlapperPositionChangedSpy.count, 0)
            // release mouse
            mouseRelease(map, 5, 25)
            // TODO enable these!
            compare(mouseUpperEnteredSpy.count, 5)
            compare(mouseUpperExitedSpy.count, 5) // release triggers one more exited()
        }

        function test_basic_press_release() {
            clear_data()
            wait(500);
            // send to emptiness
            mousePress(map, 5, 5)
            compare(mouseUpperPressedSpy.count, 0)
            compare(mouseLowerPressedSpy.count, 0)
            compare(mouseOverlapperPressedSpy.count, 0)
            mouseRelease(map, 5, 5)
            compare(mouseUpperReleasedSpy.count, 0)
            compare(mouseLowerReleasedSpy.count, 0)
            compare(mouseOverlapperReleasedSpy.count, 0)
            // send to upper mouse area
            mousePress(map, 5, 25)
            compare(mouseUpperPressedSpy.count, 1)
            compare(mouseLowerPressedSpy.count, 0)
            compare(mouseOverlapperPressedSpy.count, 0)

            compare(mouseUpper.lastAccepted, true)
            compare(mouseUpper.lastButton, Qt.LeftButton)
            compare(mouseUpper.lastModifiers, Qt.NoModifier)
            compare(mouseUpper.lastWasHeld, false)
            compare(mouseUpper.lastX, 5)
            compare(mouseUpper.lastY, 5) // remember 20 offset of the mouse area

            mouseRelease(map, 5, 25)
            compare(mouseUpperPressedSpy.count, 1)
            compare(mouseUpperReleasedSpy.count, 1)
            compare(mouseLowerPressedSpy.count, 0)
            compare(mouseLowerReleasedSpy.count, 0)

            mousePress(map, 5, 26)
            compare(mouseUpperPressedSpy.count, 2)
            compare(mouseLowerPressedSpy.count, 0)
            compare(mouseOverlapperPressedSpy.count, 0)

            mouseRelease(map, 5, 26)
            compare(mouseUpperPressedSpy.count, 2)
            compare(mouseUpperReleasedSpy.count, 2)
            compare(mouseLowerPressedSpy.count, 0)
            compare(mouseLowerReleasedSpy.count, 0)
            compare(mouseUpper.lastAccepted, true)
            compare(mouseUpper.lastButton, Qt.LeftButton)
            compare(mouseUpper.lastModifiers, Qt.NoModifier)
            compare(mouseUpper.lastWasHeld, false)
            compare(mouseUpper.lastX, 5)
            compare(mouseUpper.lastY, 6) // remember 20 offset of the mouse area

            mousePress(map, 5, 75)
            compare(mouseUpperPressedSpy.count, 2)
            compare(mouseLowerPressedSpy.count, 1)
            compare(mouseOverlapperPressedSpy.count, 0)
            compare(mouseLower.lastAccepted, true)
            compare(mouseLower.lastButton, Qt.LeftButton)
            compare(mouseLower.lastModifiers, Qt.NoModifier)
            compare(mouseLower.lastWasHeld, false)
            compare(mouseLower.lastX, 5)
            compare(mouseLower.lastY, 25) // remember 50 offset of the mouse area

            mouseRelease(map, 5, 75)
            compare(mouseUpperPressedSpy.count, 2)
            compare(mouseUpperReleasedSpy.count, 2)
            compare(mouseLowerPressedSpy.count, 1)
            compare(mouseLowerReleasedSpy.count, 1)


            compare(mouseOverlapperPressedSpy.count, 0)
            mousePress(map, 55, 75)
            compare(mouseUpperPressedSpy.count, 2)
            compare(mouseLowerPressedSpy.count, 1)
            compare(mouseOverlapperPressedSpy.count, 1)
            compare(mouseOverlapperReleasedSpy.count, 0)
            mouseRelease(map, 55, 25)
            compare(mouseUpperPressedSpy.count, 2)
            compare(mouseUpperReleasedSpy.count, 2)
            compare(mouseLowerPressedSpy.count, 1)
            compare(mouseLowerReleasedSpy.count, 1)
            //this should follow the same logic as Flickable
            compare(mouseOverlapperReleasedSpy.count, 0)
        }

        function test_basic_click() {
            clear_data();
            wait(500);

            mouseClick(map, 5, 5, Qt.RightButton, Qt.AltModifier)
            compare(mouseUpperClickedSpy.count, 0)
            compare(mouseLowerClickedSpy.count, 0)
            compare(mouseOverlapperClickedSpy.count, 0)
            mouseUpper.acceptedButtons = Qt.LeftButton | Qt.RightButton
            // TC sending click event to upper mouse area 5,25
            mouseClick(map, 5, 25, Qt.RightButton, Qt.AltModifier)
            tryCompare(mouseUpperClickedSpy, "count", 1)
            // TC done and clicked was received
            //compare(mouseUpperClickedSpy.count, 1)
            compare(mouseLowerClickedSpy.count, 0)
            compare(mouseOverlapperClickedSpy.count, 0)
            compare(mouseUpper.lastAccepted, true)
            compare(mouseUpper.lastButton, Qt.RightButton)
            compare(mouseUpper.lastModifiers, Qt.AltModifier)
            compare(mouseUpper.lastWasHeld, false)
            compare(mouseUpper.lastX, 5)
            compare(mouseUpper.lastY, 5) // remember 20 offset of the mouse area
            // check we get valid geocoordinates (would be NaN if something was wrong)
            // todo
            //verify(mouseUpper.lastMouseEvent.coordinate.longitude > -180 && mouseUpper.lastMouseEvent.coordinate.longitude < 180)
            //verify(mouseUpper.lastMouseEvent.coordinate.longitude > -90 && mouseUpper.lastMouseEvent.coordinate.latitude < 90)

            // mouse click with unaccepted buttons should not cause click
            mouseUpper.acceptedButtons = Qt.LeftButton
            mouseClick(map, 5, 25, Qt.RightButton, Qt.AltModifier)
            tryCompare(mouseUpperClickedSpy, "count", 1)
            compare(mouseLowerClickedSpy.count, 0)
            compare(mouseOverlapperClickedSpy.count, 0)

            mouseClick(map, 5, 25)
            tryCompare(mouseUpperClickedSpy, "count", 2)
            compare(mouseLowerClickedSpy.count, 0)
            compare(mouseOverlapperClickedSpy.count, 0)
            compare(mouseUpper.lastModifiers, Qt.NoModifier)
            compare(mouseUpper.lastButton, Qt.LeftButton)
            mouseClick(map, 5, 55)
            tryCompare(mouseUpperClickedSpy, "count", 2)
            compare(mouseLowerClickedSpy.count, 1)
            compare(mouseOverlapperClickedSpy.count, 0)
            mouseClick(map, 5, 55)
            tryCompare(mouseUpperClickedSpy,"count", 2)
            compare(mouseLowerClickedSpy.count, 2)
            compare(mouseOverlapperClickedSpy.count, 0)
            // declaration order counts on overlap case; overlapping area
            // declared later will get the events
            mouseClick(map, 55, 25)
            tryCompare(mouseUpperClickedSpy, "count", 2)
            compare(mouseLowerClickedSpy.count, 2)
            compare(mouseOverlapperClickedSpy.count, 1)
            mouseClick(map, 55, 75)
            tryCompare(mouseUpperClickedSpy, "count", 2)
            compare(mouseLowerClickedSpy.count, 2)
            compare(mouseOverlapperClickedSpy.count, 2)
            real_click(map, 55, 25)
            tryCompare(mouseUpperClickedSpy, "count", 2)
            compare(mouseLowerClickedSpy.count, 2)
            compare(mouseOverlapperClickedSpy.count, 3)
            real_click(map, 55, 75)
            tryCompare(mouseUpperClickedSpy, "count", 2)
            compare(mouseLowerClickedSpy.count, 2)
            compare(mouseOverlapperClickedSpy.count, 4)
        }

        function test_basic_double_click() {
            clear_data();
            wait(500);
            real_double_click(map, 5, 5)

            compare(mouseUpperDoubleClickedSpy.count, 0)
            compare(mouseLowerDoubleClickedSpy.count, 0)
            compare(mouseOverlapperDoubleClickedSpy.count, 0)
            real_double_click(map, 5, 25)
            tryCompare(mouseUpper, "lastAccepted", true)
            compare(mouseUpper.lastButton, Qt.LeftButton)
            compare(mouseUpper.lastModifiers, Qt.NoModifier)
            compare(mouseUpper.lastWasHeld, false)
            compare(mouseUpper.lastX, 5)
            compare(mouseUpper.lastY, 5) // remember 20 offset of the mouse area

            compare(mouseUpperDoubleClickedSpy.count, 1)
            compare(mouseLowerDoubleClickedSpy.count, 0)
            compare(mouseOverlapperDoubleClickedSpy.count, 0)
            real_double_click(map, 5, 25)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 2)
            compare(mouseLowerDoubleClickedSpy.count, 0)
            compare(mouseOverlapperDoubleClickedSpy.count, 0)
            real_double_click(map, 5, 55)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 2)
            compare(mouseLowerDoubleClickedSpy.count, 1)
            compare(mouseOverlapperDoubleClickedSpy.count, 0)
            real_double_click(map, 5, 55)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 2)
            compare(mouseLowerDoubleClickedSpy.count, 2)
            compare(mouseOverlapperDoubleClickedSpy.count, 0)
            // declaration order counts on overlap case; overlapping area declared later will get the events
            real_double_click(map, 55, 25)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 2)
            compare(mouseLowerDoubleClickedSpy.count, 2)
            compare(mouseOverlapperDoubleClickedSpy.count, 1)
            compare(mouseOverlapperPressedSpy.count, 2)
            compare(mouseOverlapperReleasedSpy.count, 2)
            real_double_click(map, 55, 75)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 2)
            compare(mouseLowerDoubleClickedSpy.count, 2)
            compare(mouseOverlapperDoubleClickedSpy.count, 2)
            compare(mouseOverlapperPressedSpy.count, 4)
            compare(mouseOverlapperReleasedSpy.count, 4)
            // disable overlapping area and check event is delivered to the ones beneath
            mouseOverlapper.enabled = false
            real_double_click(map, 55, 25)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 3)
            compare(mouseLowerDoubleClickedSpy.count, 2)
            compare(mouseOverlapperDoubleClickedSpy.count, 2)
            real_double_click(map, 55, 75)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 3)
            compare(mouseLowerDoubleClickedSpy.count, 3)
            compare(mouseOverlapperDoubleClickedSpy.count, 2)
            mouseOverlapper.enabled = true
            real_double_click(map, 55, 25)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 3)
            compare(mouseLowerDoubleClickedSpy.count, 3)
            compare(mouseOverlapperDoubleClickedSpy.count, 3)
            real_double_click(map, 55, 75)
            tryCompare(mouseUpperDoubleClickedSpy, "count", 3)
            compare(mouseLowerDoubleClickedSpy.count, 3)
            compare(mouseOverlapperDoubleClickedSpy.count, 4)
        }

        function test_zzz_basic_press_and_hold() { // _zzz_ to ensure execution last (takes time)
            clear_data();
            wait(1000);
            real_press_and_hold(map, 5, 5)
            compare(mouseUpperPressAndHoldSpy.count, 0)
            compare(mouseLowerPressAndHoldSpy.count, 0)
            compare(mouseOverlapperPressAndHoldSpy.count, 0)

            mousePress(map,5,25)
            wait(1000) // threshold is 800 ms
            compare(mouseUpperPressAndHoldSpy.count, 1)
            compare(mouseLowerPressAndHoldSpy.count, 0)
            compare(mouseOverlapperPressAndHoldSpy.count, 0)
            compare(mouseUpper.lastAccepted, true)
            compare(mouseUpper.lastButton, Qt.LeftButton)
            compare(mouseUpper.lastModifiers, Qt.NoModifier)
            compare(mouseUpper.lastWasHeld, true) // notable part
            compare(mouseUpper.lastX, 5)
            compare(mouseUpper.lastY, 5) // remember 20 offset of the mouse area
            mouseRelease(map,5,25)
            real_press_and_hold(map, 5, 55)
            tryCompare(mouseUpperPressAndHoldSpy, "count", 1)
            compare(mouseLowerPressAndHoldSpy.count, 1)
            compare(mouseOverlapperPressAndHoldSpy.count, 0)
            real_press_and_hold(map, 55, 75)
            tryCompare(mouseUpperPressAndHoldSpy, "count", 1)
            compare(mouseLowerPressAndHoldSpy.count, 1)
            compare(mouseOverlapperPressAndHoldSpy.count, 1)
            compare(mouseOverlapper.lastAccepted, true)
            compare(mouseOverlapper.lastButton, Qt.LeftButton)
            compare(mouseOverlapper.lastModifiers, Qt.NoModifier)
            compare(mouseOverlapper.lastWasHeld, true)
            compare(mouseOverlapper.lastX, 5)
            compare(mouseOverlapper.lastY, 75)
            // make sure that the wasHeld is cleared
            mouseClick(map, 55, 75)
            tryCompare(mouseOverlapper, "lastAccepted", true)
            compare(mouseOverlapper.lastButton, Qt.LeftButton)
            compare(mouseOverlapper.lastModifiers, Qt.NoModifier)
            compare(mouseOverlapper.lastWasHeld, false)
            compare(mouseOverlapper.lastX, 5)
            compare(mouseOverlapper.lastY, 75)
            real_press_and_hold(map, 55, 25)
            tryCompare(mouseUpperPressAndHoldSpy, "count", 1)
            compare(mouseLowerPressAndHoldSpy.count, 1)
            compare(mouseOverlapperPressAndHoldSpy.count, 2)
        }
    }
}
