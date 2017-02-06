/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

import QtQuick 2.2
import QtTest 1.0
import QtQuick.Controls 2.1

TestCase {
    id: testCase
    width: 400
    height: 400
    visible: true
    when: windowShown
    name: "ScrollIndicator"

    Component {
        id: scrollIndicator
        ScrollIndicator { }
    }

    Component {
        id: flickable
        Flickable {
            width: 100
            height: 100
            contentWidth: 200
            contentHeight: 200
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalAndVerticalFlick
        }
    }

    function test_attach() {
        var container = flickable.createObject(testCase)
        verify(container)
        waitForRendering(container)

        var vertical = scrollIndicator.createObject()
        verify(!vertical.parent)
        compare(vertical.size, 0.0)
        compare(vertical.position, 0.0)
        compare(vertical.active, false)
        compare(vertical.orientation, Qt.Vertical)
        compare(vertical.x, 0)
        compare(vertical.y, 0)
        verify(vertical.width > 0)
        verify(vertical.height > 0)

        container.ScrollIndicator.vertical = vertical
        compare(vertical.parent, container)
        compare(vertical.orientation, Qt.Vertical)
        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(vertical.x, container.width - vertical.width)
        compare(vertical.y, 0)
        verify(vertical.width > 0)
        compare(vertical.height, container.height)
        // vertical scroll indicator follows flickable's width
        container.width += 10
        compare(vertical.x, container.width - vertical.width)
        // ...unless explicitly positioned
        vertical.x = 123
        container.width += 10
        compare(vertical.x, 123)

        var horizontal = scrollIndicator.createObject()
        verify(!horizontal.parent)
        compare(horizontal.size, 0.0)
        compare(horizontal.position, 0.0)
        compare(horizontal.active, false)
        compare(horizontal.orientation, Qt.Vertical)
        compare(horizontal.x, 0)
        compare(horizontal.y, 0)
        verify(horizontal.width > 0)
        verify(horizontal.height > 0)

        container.ScrollIndicator.horizontal = horizontal
        compare(horizontal.parent, container)
        compare(horizontal.orientation, Qt.Horizontal)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)
        compare(horizontal.x, 0)
        compare(horizontal.y, container.height - horizontal.height)
        compare(horizontal.width, container.width)
        verify(horizontal.height > 0)
        // horizontal scroll indicator follows flickable's height
        container.height += 10
        compare(horizontal.y, container.height - horizontal.height)
        // ...unless explicitly positioned
        horizontal.y = 123
        container.height += 10
        compare(horizontal.y, 123)

        var velocity = container.maximumFlickVelocity

        compare(container.flicking, false)
        container.flick(-velocity, -velocity)
        compare(container.flicking, true)
        tryCompare(container, "flicking", false)

        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)

        compare(container.flicking, false)
        container.flick(velocity, velocity)
        compare(container.flicking, true)
        tryCompare(container, "flicking", false)

        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)

        var oldY = vertical.y
        var oldHeight = vertical.height
        vertical.parent = testCase
        vertical.y -= 10
        container.height += 10
        compare(vertical.y, oldY - 10)
        compare(vertical.height, oldHeight)

        var oldX = horizontal.x
        var oldWidth = horizontal.width
        horizontal.parent = testCase
        horizontal.x -= 10
        container.width += 10
        compare(horizontal.x, oldX - 10)
        compare(horizontal.width, oldWidth)

        container.destroy()
    }

    function test_warning() {
        ignoreWarning(Qt.resolvedUrl("tst_scrollindicator.qml") + ":45:1: QML TestCase: ScrollIndicator must be attached to a Flickable")
        testCase.ScrollIndicator.vertical = null
    }

    function test_overshoot() {
        var container = flickable.createObject(testCase)
        verify(container)
        waitForRendering(container)

        var vertical = scrollIndicator.createObject(container, {size: 0.5})
        container.ScrollIndicator.vertical = vertical

        var horizontal = scrollIndicator.createObject(container, {size: 0.5})
        container.ScrollIndicator.horizontal = horizontal

        // negative vertical overshoot (pos < 0)
        vertical.position = -0.1
        compare(vertical.contentItem.y, vertical.topPadding)
        compare(vertical.contentItem.height, 0.4 * vertical.availableHeight)

        // positive vertical overshoot (pos + size > 1)
        vertical.position = 0.8
        compare(vertical.contentItem.y, vertical.topPadding + 0.8 * vertical.availableHeight)
        compare(vertical.contentItem.height, 0.2 * vertical.availableHeight)

        // negative horizontal overshoot (pos < 0)
        horizontal.position = -0.1
        compare(horizontal.contentItem.x, horizontal.leftPadding)
        compare(horizontal.contentItem.width, 0.4 * horizontal.availableWidth)

        // positive horizontal overshoot (pos + size > 1)
        horizontal.position = 0.8
        compare(horizontal.contentItem.x, horizontal.leftPadding + 0.8 * horizontal.availableWidth)
        compare(horizontal.contentItem.width, 0.2 * horizontal.availableWidth)

        container.destroy()
    }
}
