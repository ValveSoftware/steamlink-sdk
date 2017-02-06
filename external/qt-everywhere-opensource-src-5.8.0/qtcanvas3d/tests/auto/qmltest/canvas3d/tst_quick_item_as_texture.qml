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

import QtQuick 2.2
import QtTest 1.0

import "tst_quick_item_as_texture.js" as Content

Item {
    id: top
    height: 300
    width: 300

    property var canvas3d: null
    property var shaderEffectSource: null
    property var shaderEffectSource2: null
    property var activeContent: Content
    property bool initOk: false
    property bool renderOk: false
    property var canvasWindow: null
    property bool windowHidden: false
    property int xpos: 150
    property int ypos: 150
    property var pixels
    property int red: -1
    property int green: -1
    property int blue: -1
    property int alpha: -1

    Rectangle {
        id: testRect
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 64
        height: 64
        color: "#102030"
        z:1
    }

    Rectangle {
        id: testRect2
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 64
        height: 64
        color: "#122232"
        z:1
    }

    Rectangle {
        id: testRect3
        anchors.top: parent.top
        anchors.right: parent.right
        width: 64
        height: 64
        color: "#132333"
        z:1
    }

    function createCanvas() {
        canvas3d = Qt.createQmlObject("
        import QtQuick 2.2
        import QtCanvas3D 1.1
        Canvas3D {
            onInitializeGL: {
                activeContent.initializeGL(this)
                initOk = true;
            }
            onPaintGL: {
                pixels = activeContent.paintGL(xpos, ypos)
                red = pixels[0]
                green = pixels[1]
                blue = pixels[2]
                alpha = pixels[3]
                renderOk = true
            }
        }", top)
        canvas3d.anchors.fill = top
    }

    function createShaderEffectSource(source) {
        var effect = Qt.createQmlObject("
        import QtQuick 2.2
        ShaderEffectSource {
            width: 64
            height: 64
            live: false
            hideSource: false
            mipmap: true
            z: 1
        }", top)
        effect.sourceItem = source
        return effect
    }

    function resetRenderCheck() {
        renderOk = false
        red = -1
        green = -1
        blue = -1
        alpha = -1
        activeContent.resetReadyTextures()
    }

    TestCase {
        name: "Canvas3D_quick_item_as_texture"
        when: windowShown

        function test_quick_item_as_texture_1() {
            verify(canvas3d === null)
            verify(initOk === false)
            verify(renderOk === false)
            createCanvas()
            verify(canvas3d !== null)
            waitForRendering(canvas3d)

            tryCompare(top, "initOk", true)

            shaderEffectSource = createShaderEffectSource(testRect)
            shaderEffectSource2 = createShaderEffectSource(testRect2)
            testRect3.layer.enabled = true
            verify(shaderEffectSource !== null)
            verify(shaderEffectSource2 !== null)

            resetRenderCheck()
            activeContent.updateTexture(shaderEffectSource)
            waitForRendering(canvas3d)

            tryCompare(activeContent.readyTextures, "length", 1)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x10)
            tryCompare(top, "green", 0x20)
            tryCompare(top, "blue", 0x30)
            tryCompare(top, "alpha", 0xff)

            resetRenderCheck()
            activeContent.updateTexture(shaderEffectSource2)
            waitForRendering(canvas3d)

            tryCompare(activeContent.readyTextures, "length", 1)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x12)
            tryCompare(top, "green", 0x22)
            tryCompare(top, "blue", 0x32)
            tryCompare(top, "alpha", 0xff)

            // testRect3 is layered
            resetRenderCheck()
            activeContent.updateTexture(testRect3)
            tryCompare(activeContent.readyTextures, "length", 1)
            waitForRendering(canvas3d)

            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x13)
            tryCompare(top, "green", 0x23)
            tryCompare(top, "blue", 0x33)
            tryCompare(top, "alpha", 0xff)

            // Texture is not live, color should not change
            testRect.color = "#405060"
            resetRenderCheck()
            activeContent.updateTexture(shaderEffectSource)
            tryCompare(activeContent.readyTextures, "length", 1)

            waitForRendering(canvas3d)

            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x10)
            tryCompare(top, "green", 0x20)
            tryCompare(top, "blue", 0x30)
            tryCompare(top, "alpha", 0xff)

            // Turn live on, color will change to what we previously set it
            resetRenderCheck()
            shaderEffectSource.live = true

            waitForRendering(canvas3d)

            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x40)
            tryCompare(top, "green", 0x50)
            tryCompare(top, "blue", 0x60)
            tryCompare(top, "alpha", 0xff)

            // Live texture, new color
            testRect.color = "#AABBCC"
            resetRenderCheck()

            waitForRendering(canvas3d)

            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0xAA)
            tryCompare(top, "green", 0xBB)
            tryCompare(top, "blue", 0xCC)
            tryCompare(top, "alpha", 0xff)

            // Destroy shaderEffectSources
            shaderEffectSource.destroy()
            shaderEffectSource2.destroy()
            testRect3.layer.enabled = false

            waitForRendering(canvas3d)

            testRect.color = "#718191"
            testRect2.color = "#728292"
            testRect3.color = "#738393"

            waitForRendering(canvas3d)

            // Recreate shaderEffectSources
            shaderEffectSource = createShaderEffectSource(testRect)
            shaderEffectSource2 = createShaderEffectSource(testRect2)
            testRect3.layer.enabled = true
            verify(shaderEffectSource !== null)
            verify(shaderEffectSource2 !== null)

            waitForRendering(canvas3d)

            // Test that each textureProvider is valid second time around
            activeContent.updateTexture(shaderEffectSource)

            resetRenderCheck()
            waitForRendering(canvas3d)

            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x71)
            tryCompare(top, "green", 0x81)
            tryCompare(top, "blue", 0x91)
            tryCompare(top, "alpha", 0xff)

            activeContent.updateTexture(shaderEffectSource2)

            resetRenderCheck()
            waitForRendering(canvas3d)

            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x72)
            tryCompare(top, "green", 0x82)
            tryCompare(top, "blue", 0x92)
            tryCompare(top, "alpha", 0xff)

            activeContent.updateTexture(testRect3)

            resetRenderCheck()
            waitForRendering(canvas3d)

            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x73)
            tryCompare(top, "green", 0x83)
            tryCompare(top, "blue", 0x93)
            tryCompare(top, "alpha", 0xff)

            shaderEffectSource.destroy()
            shaderEffectSource2.destroy()
            testRect3.layer.enabled = false

            waitForRendering(canvas3d)

            testRect.layer.enabled = true
            testRect2.layer.enabled = true
            testRect3.layer.enabled = true

            resetRenderCheck()
            waitForRendering(canvas3d)

            // Test that multiple items each trigger textureReady signal
            activeContent.updateTexture(testRect)
            activeContent.updateTexture(testRect2)
            activeContent.updateTexture(testRect3)

            tryCompare(activeContent.readyTextures, "length", 3)
        }
    }
}
