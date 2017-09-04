/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

import QtQuick 2.7
import QtQuick.Window 2.2

Item {
    id: appleTVInput

    property real xThreshold: 500
    property real yThreshold: 300

    signal swipeLeft()
    signal swipeRight()
    signal swipeUp()
    signal swipeDown()

    onSwipeLeft: {
        var focusedItem = Window.window.activeFocusItem
        if (typeof(focusedItem.swipeLeft) === "function") {
            focusedItem.swipeLeft()
        }
    }

    onSwipeRight: {
        var focusedItem = Window.window.activeFocusItem
        if (typeof(focusedItem.swipeRight) === "function") {
            focusedItem.swipeRight()
        }
    }

    onSwipeUp: {
        var focusedItem = Window.window.activeFocusItem
        if (typeof(focusedItem.swipeUp) === "function") {
            focusedItem.swipeUp()
        }
    }

    onSwipeDown: {
        var focusedItem = Window.window.activeFocusItem
        if (typeof(focusedItem.swipeDown) === "function") {
            focusedItem.swipeDown()
        }
    }

    MultiPointTouchArea {
        id: multiPointTouchArea

        anchors.fill: parent

        property int startX: 0
        property int startY: 0

        property int pointX: 0
        property int pointY: 0

        property int xIsIncreasing: -1 // 1 - true, 0 - false, -1 - undefined
        property int yIsIncreasing: -1 // 1 - true, 0 - false, -1 - undefined

        property bool pressed: false

        touchPoints: [
            TouchPoint {

                onPressedChanged: {
                    if (pressed) {
                        multiPointTouchArea.startX = x
                        multiPointTouchArea.startY = y
                        multiPointTouchArea.pressed = true
                    } else {
                        multiPointTouchArea.pressed = false
                        multiPointTouchArea.xIsIncreasing = -1
                        multiPointTouchArea.yIsIncreasing = -1
                        multiPointTouchArea.startX = 0
                        multiPointTouchArea.startY = 0
                    }
                }

                onXChanged: {
                    if (multiPointTouchArea.pressed) {
                        var newValue = x - multiPointTouchArea.startX

                        var focusedItem = appleTVInput.Window.window.activeFocusItem

                        if (focusedItem.panningEnabled) {
                            if (multiPointTouchArea.xIsIncreasing === -1) {
                                multiPointTouchArea.xIsIncreasing = (newValue > multiPointTouchArea.startX) ? 1 : 0
                            } else {
                                if (multiPointTouchArea.xIsIncreasing === 1 && newValue < multiPointTouchArea.pointX) {
                                    multiPointTouchArea.startX = x
                                    multiPointTouchArea.xIsIncreasing = 0
                                    multiPointTouchArea.pointX = newValue
                                } else if (multiPointTouchArea.xIsIncreasing === 0 && newValue > multiPointTouchArea.pointX) {
                                    multiPointTouchArea.startX = x
                                    multiPointTouchArea.xIsIncreasing = 1
                                    multiPointTouchArea.pointX = newValue
                                } else {
                                    multiPointTouchArea.pointX = newValue
                                    focusedItem.pannedHorizontally(multiPointTouchArea.pointX / (multiPointTouchArea.width / 2))
                                }
                            }
                        } else {
                            multiPointTouchArea.pointX = newValue

                            if (multiPointTouchArea.pointX > xThreshold) {
                                multiPointTouchArea.startX = x
                                swipeRight()
                            } else if (multiPointTouchArea.pointX < -xThreshold) {
                                multiPointTouchArea.startX = x
                                swipeLeft()
                            }
                        }
                    }
                }

                onYChanged: {
                    if (multiPointTouchArea.pressed) {
                        var newValue = multiPointTouchArea.startY - y

                        var focusedItem = appleTVInput.Window.window.activeFocusItem

                        if (focusedItem.panningEnabled) {
                            if (multiPointTouchArea.yIsIncreasing === -1) {
                                multiPointTouchArea.yIsIncreasing = (newValue > multiPointTouchArea.startY) ? 1 : 0
                            } else {
                                if (multiPointTouchArea.yIsIncreasing === 1 && newValue < multiPointTouchArea.pointY) {
                                    multiPointTouchArea.startY = y
                                    multiPointTouchArea.yIsIncreasing = 0
                                    multiPointTouchArea.pointY = newValue
                                } else if (multiPointTouchArea.yIsIncreasing === 0 && newValue > multiPointTouchArea.pointY) {
                                    multiPointTouchArea.startY = y
                                    multiPointTouchArea.yIsIncreasing = 1
                                    multiPointTouchArea.pointY = newValue
                                } else {
                                    multiPointTouchArea.pointY = newValue
                                    focusedItem.pannedVertically(multiPointTouchArea.pointY / (multiPointTouchArea.height / 2))
                                }
                            }
                        } else {
                            multiPointTouchArea.pointY = newValue

                            if (multiPointTouchArea.pointY > yThreshold) {
                                multiPointTouchArea.startY = y
                                swipeUp()
                            } else if (multiPointTouchArea.pointY < -yThreshold) {
                                multiPointTouchArea.startY = y
                                swipeDown()
                            }
                        }
                    }
                }
            }
        ]
    }
}
