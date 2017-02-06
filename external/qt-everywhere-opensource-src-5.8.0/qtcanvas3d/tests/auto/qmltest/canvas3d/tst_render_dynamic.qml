/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

import QtQuick 2.6
import QtCanvas3D 1.1
import QtTest 1.1
import QtQuick.Window 2.0

import "tst_render_simple.js" as Content

Item {
    id: top
    height: 200
    width: 200

    property var canvas3d: null
    property var activeContent: Content
    property bool initOk: false
    property bool renderOk: false
    property var canvasWindow: null
    property bool windowHidden: false
    property var window1: null
    property var window2: null

    function createCanvas(parentItem) {
        canvas3d = Qt.createQmlObject("
        import QtQuick 2.2
        import QtCanvas3D 1.1
        Canvas3D {
            onInitializeGL: initOk = activeContent.initializeGL(canvas3d)
            onPaintGL: {
                renderOk = true
                activeContent.paintGL(canvas3d)
            }
        }", parentItem)
        canvas3d.anchors.fill = parentItem
    }

    function createCanvasWindow() {
        canvasWindow = Qt.createQmlObject("
        import QtQuick 2.2
        import QtCanvas3D 1.1
        import QtQuick.Window 2.0
        Window {
            property alias windowCanvas: theCanvas
            visible: true
            width: 200
            height: 200
            x: top.width
            y: top.height
            Canvas3D {
                id: theCanvas
                anchors.fill: parent
                onInitializeGL: initOk = activeContent.initializeGL(theCanvas)
                onPaintGL: {
                    renderOk = true
                    activeContent.paintGL(theCanvas)
                }
            }
            onVisibleChanged: {
                windowHidden = !visible
            }
        }", top)
    }

    function createCanvasNoParent() {
        var canvasComponent = Qt.createComponent("tst_render_dynamic_canvas_component.qml");
        if (canvasComponent.status === Component.Ready) {
            canvas3d = canvasComponent.createObject(null)
        }
    }

    function createWindow() {
        return Qt.createQmlObject("
        import QtQuick 2.2
        import QtCanvas3D 1.0
        import QtQuick.Window 2.0
        Window {
            property alias windowTop: windowTop
            visible: true
            width: 200
            height: 200
            Item {
                id: windowTop
                anchors.fill: parent
            }
        }", top)
    }

    function createWindow1() {
        window1 = createWindow();
        window1.x = top.x + top.width
        window1.y = top.y
    }
    function createWindow2() {
        window2 = createWindow();
        window2.x = window1.x + window1.width
        window2.y = window1.y
    }

    function checkCanvasCorrect(windowTop) {
        var image = theTestCase.grabImage(windowTop);
        theTestCase.verify(image.pixel(5, 10) === Qt.rgba(0, 0, 1, 1))
        theTestCase.verify(image.pixel(5, 190) === Qt.rgba(0, 0, 1, 1))
        theTestCase.verify(image.pixel(95, 100) === Qt.rgba(0, 0, 1, 1))
        theTestCase.verify(image.pixel(195, 10) === Qt.rgba(0, 0, 0, 1))
        theTestCase.verify(image.pixel(195, 190) === Qt.rgba(0, 0, 0, 1))
        theTestCase.verify(image.pixel(105, 100) === Qt.rgba(0, 0, 0, 1))
    }

    TestCase {
        id: theTestCase
        name: "Canvas3D_render_dynamic"
        when: windowShown

        function test_render_01_dynamic_creation() {
            verify(canvas3d === null)
            verify(initOk === false)
            verify(renderOk === false)
            createCanvas(top)
            verify(canvas3d !== null)
            waitForRendering(canvas3d)
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
            checkCanvasCorrect(top)
        }

        function test_render_02_dynamic_deletion() {
            verify(canvas3d !== null)
            verify(initOk === true)
            verify(renderOk === true)
            canvas3d.destroy()
            waitForRendering(canvas3d)
            verify(canvas3d === null)
        }

        function test_render_03_dynamic_recreation() {
            initOk = false
            renderOk = false
            createCanvas(top)
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
            waitForRendering(canvas3d)
            verify(canvas3d !== null)
            checkCanvasCorrect(top)

            canvas3d.destroy()
            waitForRendering(canvas3d)
            verify(canvas3d === null)

            initOk = false
            renderOk = false
            createCanvas(top)
            waitForRendering(canvas3d)
            verify(canvas3d !== null)
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
            checkCanvasCorrect(top)

            canvas3d.destroy()
            waitForRendering(canvas3d)
            verify(canvas3d === null)
        }

        function test_render_04_dynamic_window_creation() {
            initOk = false
            renderOk = false
            verify(canvasWindow === null)
            createCanvasWindow()
            verify(canvasWindow !== null)
            waitForRendering(canvasWindow.windowCanvas)
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "windowHidden", false)
            checkCanvasCorrect(canvasWindow.windowCanvas)
            canvasWindow.destroy()
            tryCompare(top, "canvasWindow", null)
        }

        function test_render_05_dynamic_window_hide_and_reshow() {
            initOk = false
            renderOk = false
            verify(canvasWindow === null)
            verify(windowHidden === false)
            createCanvasWindow()
            verify(canvasWindow !== null)
            waitForRendering(canvasWindow.windowCanvas)
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "windowHidden", false)
            checkCanvasCorrect(canvasWindow.windowCanvas)
            canvasWindow.hide()
            wait(1000) // Short wait to allow events to be processed
            tryCompare(top, "windowHidden", true)
            renderOk = false
            canvasWindow.show()
            waitForRendering(canvasWindow.windowCanvas)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "windowHidden", false)
            checkCanvasCorrect(canvasWindow.windowCanvas)
            canvasWindow.destroy()
            tryCompare(top, "canvasWindow", null)
        }

        function test_render_06_dynamic_no_parent() {
            verify(canvas3d === null)
            createCanvasNoParent()
            verify(canvas3d !== null)
            verify(canvas3d.parent === null)
            canvas3d.destroy()
            tryCompare(top, "canvas3d", null)
        }

        function test_render_07_dynamic_from_no_parent_to_parent() {
            verify(canvas3d === null)
            createCanvasNoParent()
            verify(canvas3d !== null)
            verify(canvas3d.parent === null)
            canvas3d.parent = top
            waitForRendering(canvas3d)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            checkCanvasCorrect(top)
            canvas3d.destroy()
            tryCompare(top, "canvas3d", null)
        }

        function test_render_08_dynamic_from_no_parent_to_parent_and_back() {
            verify(canvas3d === null)
            createCanvasNoParent()
            verify(canvas3d !== null)
            verify(canvas3d.parent === null)

            // Canvas to window
            canvas3d.parent = top
            waitForRendering(canvas3d)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            checkCanvasCorrect(top)

            // Remove canvas from window
            canvas3d.parent = null
            waitForRendering(top)

            // Canvas back to same window - should not reinitialize
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.parent = top
            waitForRendering(top)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "initOk", false)
            tryCompare(canvas3d, "contextLostOk", false)
            checkCanvasCorrect(top)

            // Remove canvas from window
            canvas3d.parent = null
            waitForRendering(top)

            // Destroy canvas
            canvas3d.destroy()
            tryCompare(top, "canvas3d", null)

            // Create canvas again
            createCanvasNoParent()
            verify(canvas3d !== null)
            verify(canvas3d.parent === null)

            // Canvas to window
            canvas3d.parent = top
            waitForRendering(canvas3d)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", false)
            checkCanvasCorrect(top)

            // Remove canvas from window
            canvas3d.parent = null
            waitForRendering(top)

            // Canvas back to same window - should not reinitialize
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.parent = top
            waitForRendering(top)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "initOk", false)
            tryCompare(canvas3d, "contextLostOk", false)
            checkCanvasCorrect(top)

            // Destroy canvas without removing from window
            canvas3d.destroy()
            tryCompare(top, "canvas3d", null)
        }

        function test_render_09_dynamic_switch_between_two_windows() {
            verify(canvas3d === null)
            verify(window1 === null)
            verify(window2 === null)
            createCanvasNoParent()
            createWindow1()
            createWindow2()
            verify(canvas3d !== null)
            verify(window1 !== null)
            verify(window2 !== null)
            verify(canvas3d.parent === null)

            // Canvas to first window
            canvas3d.parent = window1.windowTop
            waitForRendering(window1.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", false)
            tryCompare(canvas3d, "contextRestoredOk", false)
            checkCanvasCorrect(window1.windowTop)

            // Canvas to second window
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.parent = window2.windowTop
            waitForRendering(window2.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", true)
            tryCompare(canvas3d, "contextRestoredOk", true)
            checkCanvasCorrect(window2.windowTop)

            // Canvas back to first window
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.contextLostOk = false
            canvas3d.contextRestoredOk = false
            canvas3d.parent = window1.windowTop
            waitForRendering(window1.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", true)
            tryCompare(canvas3d, "contextRestoredOk", true)
            checkCanvasCorrect(window1.windowTop)

            // Canvas to no window, destroy canvas last
            canvas3d.parent = null
            waitForRendering(window1.windowTop)
            window1.destroy()
            window2.destroy()
            canvas3d.destroy()
            tryCompare(top, "canvas3d", null)
            tryCompare(top, "window1", null)
            tryCompare(top, "window2", null)
        }

        function test_render_10_dynamic_canvas_to_window_which_is_destroyed_1() {
            verify(canvas3d === null)
            verify(window1 === null)
            verify(window2 === null)

            // Canvas is created as child of the window
            initOk = false
            renderOk = false
            createWindow1()
            createCanvas(window1.windowTop)
            verify(canvas3d !== null)
            verify(window1 !== null)
            verify(canvas3d.parent === window1.windowTop)
            waitForRendering(window1.windowTop)
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
            checkCanvasCorrect(window1.windowTop)

            // Destroy window with canvas still on it - canvas should get destroyed also
            window1.destroy()
            tryCompare(top, "window1", null)
            tryCompare(top, "canvas3d", null);
        }

        function test_render_11_dynamic_canvas_to_window_which_is_destroyed_2() {
            verify(canvas3d === null)
            verify(window1 === null)

            // Create window, this time without canvas as child
            createWindow1()
            createCanvasNoParent()
            verify(canvas3d !== null)
            verify(window1 !== null)
            verify(canvas3d.parent === null)

            // Set canvas as child of window
            canvas3d.parent = window1.windowTop
            waitForRendering(window1.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            checkCanvasCorrect(window1.windowTop)

            // Destroy window with canvas as only visual child - canvas should not get autodestroyed
            window1.destroy()
            tryCompare(top, "window1", null)
            verify(canvas3d !== null);
            canvas3d.destroy();
            tryCompare(top, "canvas3d", null);
        }

        function test_render_12_dynamic_canvas_to_window_which_is_destroyed_3() {
            verify(canvas3d === null)
            verify(window1 === null)

            // Create first window, no child
            createWindow1()
            createCanvasNoParent()
            verify(canvas3d !== null)
            verify(window1 !== null)
            verify(canvas3d.parent === null)

            // Set canvas as child of window
            canvas3d.parent = window1.windowTop
            waitForRendering(window1.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            checkCanvasCorrect(window1.windowTop)

            // Remove canvas from window
            canvas3d.parent = null
            waitForRendering(window1.windowTop)
            tryCompare(canvas3d, "contextLostOk", false)
            tryCompare(canvas3d, "contextRestoredOk", false)

            // Destroy window with canvas removed
            window1.destroy()
            tryCompare(top, "window1", null)
            verify(canvas3d !== null);
            tryCompare(canvas3d, "contextLostOk", true)
            tryCompare(canvas3d, "contextRestoredOk", false)
            var errorCheck = canvas3d.checkContextLostError()
            verify(errorCheck === true)
            canvas3d.destroy();
            tryCompare(top, "canvas3d", null);
        }

        function test_render_13_dynamic_canvas_to_window_which_is_destroyed_4() {
            verify(canvas3d === null)
            verify(window1 === null)
            verify(window2 === null)

            // Create two windows, no child
            createWindow1()
            createWindow2()
            createCanvasNoParent()
            verify(canvas3d !== null)
            verify(window1 !== null)
            verify(window2 !== null)
            verify(canvas3d.parent === null)

            // Set canvas as child of first window
            canvas3d.parent = window1.windowTop
            waitForRendering(window1.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", false)
            tryCompare(canvas3d, "contextRestoredOk", false)
            checkCanvasCorrect(window1.windowTop)

            // Remove canvas from first window
            canvas3d.parent = null
            waitForRendering(window1.windowTop)

            // Set canvas as child of second window - should reinitialize
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.parent = window2.windowTop
            waitForRendering(window2.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", true)
            tryCompare(canvas3d, "contextRestoredOk", true)
            checkCanvasCorrect(window2.windowTop)

            // Destroy first window - should not affect canvas
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.contextLostOk = false
            canvas3d.contextRestoredOk = false
            window1.destroy()
            tryCompare(top, "window1", null)
            waitForRendering(window2.windowTop)
            verify(canvas3d !== null);
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "initOk", false)
            tryCompare(canvas3d, "contextLostOk", false)
            tryCompare(canvas3d, "contextRestoredOk", false)
            checkCanvasCorrect(window2.windowTop)

            // Destroy second window - canvas only visual child, so doesn't get destroyed with it
            window2.destroy()
            tryCompare(top, "window2", null)
            tryCompare(canvas3d, "contextLostOk", true)
            tryCompare(canvas3d, "contextRestoredOk", false)
            verify(canvas3d !== null);
            canvas3d.destroy();
            tryCompare(top, "canvas3d", null);
        }

        function test_render_14_dynamic_change_window_without_render_inbetween() {
            verify(canvas3d === null)
            verify(window1 === null)
            verify(window2 === null)
            createCanvasNoParent()
            createWindow1()
            createWindow2()
            verify(canvas3d !== null)
            verify(window1 !== null)
            verify(window2 !== null)
            verify(canvas3d.parent === null)

            // Canvas to first window
            canvas3d.parent = window1.windowTop
            waitForRendering(window1.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", false)
            tryCompare(canvas3d, "contextRestoredOk", false)
            checkCanvasCorrect(window1.windowTop)

            // Toggle parent rapidly - should not affect canvas
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.contextLostOk = false
            canvas3d.contextRestoredOk = false
            canvas3d.parent = null
            canvas3d.parent = window1.windowTop
            canvas3d.parent = null
            canvas3d.parent = window1.windowTop
            canvas3d.parent = null
            canvas3d.parent = window1.windowTop
            canvas3d.parent = null
            canvas3d.parent = window1.windowTop
            tryCompare(canvas3d, "initOk", false)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", false)
            tryCompare(canvas3d, "contextRestoredOk", false)
            checkCanvasCorrect(window1.windowTop)

            // Canvas to second window
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.parent = window2.windowTop
            waitForRendering(window2.windowTop)
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", true)
            tryCompare(canvas3d, "contextRestoredOk", true)
            checkCanvasCorrect(window2.windowTop)

            // Change window rapidly - should cause context loss
            canvas3d.initOk = false
            canvas3d.renderOk = false
            canvas3d.contextLostOk = false
            canvas3d.contextRestoredOk = false
            canvas3d.parent = window2.windowTop
            canvas3d.parent = window1.windowTop
            canvas3d.parent = null
            canvas3d.parent = window1.windowTop
            canvas3d.parent = window2.windowTop
            canvas3d.parent = null
            canvas3d.parent = window1.windowTop
            canvas3d.parent = window2.windowTop
            canvas3d.parent = null
            canvas3d.parent = window2.windowTop
            canvas3d.parent = window1.windowTop
            canvas3d.parent = window2.windowTop
            tryCompare(canvas3d, "initOk", true)
            tryCompare(canvas3d, "renderOk", true)
            tryCompare(canvas3d, "contextLostOk", true)
            tryCompare(canvas3d, "contextRestoredOk", true)
            checkCanvasCorrect(window2.windowTop)

            // Destroy windows and canvas
            window1.destroy()
            window2.destroy()
            canvas3d.destroy()
            tryCompare(top, "canvas3d", null)
            tryCompare(top, "window1", null)
            tryCompare(top, "window2", null)
        }
    }
}
