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
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.2
import QtCanvas3D 1.1

Window {
    id: mainview
    width: 400
    height: 600
    visible: true
    title: "Windows:" + windowCount + " Canvases:" + canvasCount

    property var windowList: []
    property var canvasList: []
    property var windowComponent: null
    property var fboCanvasComponent: null
    property var itemTextureCanvasComponent: null
    property int windowCount: 0
    property int canvasCount: 0
    property string renderTarget: "Offscreen"

    onClosing: {
       for (var i = windowCount - 1; i >= 0; i--)
           windowList[i].close()
    }

    ColumnLayout {
        id: controlLayout
        spacing: 1
        anchors.fill: parent
        visible: true

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "New window"
            onClicked: {
                createWindow()
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "New framebuffer canvas"
            onClicked: {
                createFboCanvas(null)
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "New QuickTexture  canvas"
            onClicked: {
                createItemTextureCanvas(null)
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Delete random canvas"
            onClicked: {
                deleteCanvas(chooseRandomCanvas())
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Delete first canvas"
            onClicked: {
                if (canvasCount > 0)
                    deleteCanvas(canvasList[0])
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Delete last canvas"
            onClicked: {
                if (canvasCount > 0)
                    deleteCanvas(canvasList[canvasCount - 1])
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "RenderTarget for new canvases: " + renderTarget
            onClicked: {
                if (renderTarget === "Offscreen") {
                    renderTarget = "Background"
                } else if (renderTarget === "Background") {
                    renderTarget = "Foreground"
                } else {
                    renderTarget = "Offscreen"
                }
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Run test: Swapping two canvas in one window"
            onClicked: {
                if (singleWindowSwapTestTimer.running) {
                    if (singleWindowSwapTestCanvasFbo)
                        singleWindowSwapTestCanvasFbo.destroy()
                    if (singleWindowSwapTestCanvasQuickItem)
                        singleWindowSwapTestCanvasQuickItem.destroy()
                    if (singleWindowSwapTestWindow) {
                        removeWindow(singleWindowSwapTestWindow)
                        singleWindowSwapTestWindow.destroy()
                    }
                    singleWindowSwapTestTimer.stop()
                } else {
                    singleWindowSwapTestWindow = createWindow()
                    singleWindowSwapTestCanvasFbo = createFboCanvas(null)
                    singleWindowSwapTestCanvasQuickItem = createItemTextureCanvas(null)
                    singleWindowSwapTestTimer.start()
                }
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Run test: Swapping one canvas in two windows"
            onClicked: {
                if (doubleWindowSwapTestTimer.running) {
                    if (doubleWindowSwapTestCanvas)
                        doubleWindowSwapTestCanvas.destroy()
                    if (doubleWindowSwapTestWindow1) {
                        removeWindow(doubleWindowSwapTestWindow1)
                        doubleWindowSwapTestWindow1.destroy()
                    }
                    if (doubleWindowSwapTestWindow2) {
                        removeWindow(doubleWindowSwapTestWindow2)
                        doubleWindowSwapTestWindow2.destroy()
                    }
                    doubleWindowSwapTestTimer.stop()
                } else {
                    doubleWindowSwapTestWindow1 = createWindow()
                    doubleWindowSwapTestWindow2 = createWindow()
                    doubleWindowSwapTestCanvas = createFboCanvas(null)

                    doubleWindowSwapTestTimer.start()
                }
            }
        }
    }

    property var singleWindowSwapTestWindow: null
    property var singleWindowSwapTestCanvasFbo: null
    property var singleWindowSwapTestCanvasQuickItem: null

    Timer {
        id: singleWindowSwapTestTimer
        interval: 200
        repeat: true

        property bool fboCanvasShown: false
        property int counter: 0

        onTriggered: {
            console.log("Count:", counter++)
            if (fboCanvasShown) {
                fboCanvasShown = false
                singleWindowSwapTestCanvasQuickItem.parent = null
                singleWindowSwapTestCanvasFbo.parent = singleWindowSwapTestWindow.canvasArea
            } else {
                fboCanvasShown = true
                singleWindowSwapTestCanvasFbo.parent = null
                singleWindowSwapTestCanvasQuickItem.parent = singleWindowSwapTestWindow.canvasArea
            }
        }
    }

    property var doubleWindowSwapTestWindow1: null
    property var doubleWindowSwapTestWindow2: null
    property var doubleWindowSwapTestCanvas: null

    Timer {
        id: doubleWindowSwapTestTimer
        interval: 200
        repeat: true

        property bool firstWindow: false
        property int counter: 0

        onTriggered: {
            console.log("Count:", counter++)
            if (firstWindow) {
                firstWindow = false
                doubleWindowSwapTestCanvas.parent = doubleWindowSwapTestWindow2.canvasArea
            } else {
                firstWindow = true
                doubleWindowSwapTestCanvas.parent = doubleWindowSwapTestWindow1.canvasArea
            }
        }
    }

    function createWindow() {
        if (windowComponent === null)
            windowComponent = Qt.createComponent("canvaswindow.qml")
        var window = windowComponent.createObject(null)
        window.setManager(mainview)
        windowList[windowCount] = window
        windowCount++
        window.x = windowCount * 20
        window.y = windowCount * 20
        return window
    }

    function removeWindow(win) {
        var found = false
        for (var i = 0; i < windowCount; i++) {
            if (windowList[i] === win) {
                found = true
                windowCount--
            }
            if (found) {
                if (i < windowCount)
                    windowList[i] = windowList[i + 1]
                else
                    windowList[i] = null
            }
        }
    }

    function deleteCanvas(canvas) {
        var found = false
        for (var i = 0; i < canvasCount; i++) {
            if (canvasList[i] === canvas) {
                found = true
                canvasCount--
                console.log("Canvas deleted: ", canvasList[i].canvasName)
                canvasList[i].destroy()
            }
            if (found) {
                if (i < canvasCount)
                    canvasList[i] = canvasList[i + 1]
                else
                    canvasList[i] = null
            }
        }
    }

    function chooseRandomCanvas() {
        if (canvasCount > 0) {
            var index = Math.floor((Math.random() * canvasCount))
            console.log("Random canvas selected: ", canvasList[index].canvasName)
            return canvasList[index]
        } else {
            return null
        }
    }

    function setupCanvas(canvas, canvasArea, canvasName) {
        canvas.parent = canvasArea
        canvasList[canvasCount] = canvas
        canvasCount++
        canvas.canvasName = canvasName
        console.log("Created ", canvas.canvasName)
        if (renderTarget === "Offscreen") {
            canvas.canvas3d.renderTarget = Canvas3D.RenderTargetOffscreenBuffer
        } else if (renderTarget === "Background") {
            canvas.color = "transparent"
            canvas.canvas3d.renderTarget = Canvas3D.RenderTargetBackground
        } else {
            canvas.canvas3d.renderTarget = Canvas3D.RenderTargetForeground
        }
        return canvas
    }

    function createFboCanvas(canvasArea) {
        if (fboCanvasComponent === null)
            fboCanvasComponent = Qt.createComponent("framebuffer.qml")
        return setupCanvas(fboCanvasComponent.createObject(null), canvasArea,
                           "FBO canvas " + canvasCount)
    }

    function createItemTextureCanvas(canvasArea) {
        if (itemTextureCanvasComponent === null)
            itemTextureCanvasComponent = Qt.createComponent("quickitemtexture.qml")
        return setupCanvas(itemTextureCanvasComponent.createObject(null), canvasArea,
                           "QuickItem canvas " + canvasCount)
    }
}
