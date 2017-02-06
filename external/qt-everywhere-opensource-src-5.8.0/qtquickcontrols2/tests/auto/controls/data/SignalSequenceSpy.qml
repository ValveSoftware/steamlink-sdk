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

import QtQuick 2.5

QtObject {
    property QtObject target: null
    // We could just listen to all signals (try { signal.connect(/*...*/) } catch (e))
    // if it weren't for the fact the spy is often declared as a property of the control,
    // which creates a "spyChanged" signal, which leads to an unexpected spyChanged signal
    // emission. However, we don't know what the property will be called, so the signals
    // have to be listed explicitly.
    property var signals: []
    property var expectedSequence: []
    property int sequenceIndex: 0
    property bool sequenceFailure: false
    readonly property bool success: !sequenceFailure && sequenceIndex === expectedSequence.length

    function reset() {
        sequenceIndex = 0
        sequenceFailure = false
    }

    property QtObject __oldTarget: null
    property var __connections: []

    onExpectedSequenceChanged: reset()
    onTargetChanged: __setup()
    onSignalsChanged: __setup()

    function __setup() {
        if (__oldTarget) {
            __connections.forEach(function (cx) {
                __oldTarget[cx.name].disconnect(cx.method)
            })
            __oldTarget = null
        }

        __connections = []

        if (!!target && !!signals && signals.length > 0) {
            signals.forEach(function (signalName) {
                var handlerName = "on" + signalName.substr(0, 1).toUpperCase() + signalName.substr(1)
                var method = function() { __checkSignal(signalName, arguments) }
                target[handlerName].connect(method)
                __connections.push({ "name": handlerName, "method": method })
            })
            __oldTarget = target
        }
    }

    function __checkSignal(signalName, signalArgs) {
        if (sequenceFailure)
            return;

        if (sequenceIndex >= expectedSequence.length) {
            console.warn("SignalSequenceSpy: Received unexpected signal '" + signalName + "' (none expected).")
            sequenceFailure = true
            return
        }

        var expectedSignal = expectedSequence[sequenceIndex]
        if (typeof expectedSignal === "string") {
            if (expectedSignal === signalName) {
                sequenceIndex++
                return
            }
        } else if (typeof expectedSignal === "object") {
            var expectedSignalData = expectedSignal
            expectedSignal = expectedSignalData[0]
            if (expectedSignal === signalName) {
                var expectedValues = expectedSignalData[1]
                for (var p in expectedValues) {
                    if (target[p] != expectedValues[p]) {
                        console.warn("SignalSequenceSpy: Value mismatch for property '" + p + "' after '" + signalName + "' signal." +
                                     __mismatchValuesFormat(target[p], expectedValues[p]))
                        sequenceFailure = true
                        return
                    }
                }
                sequenceIndex++
                return
            }
        }
        console.warn("SignalSequenceSpy: Received unexpected signal (is \"" + expectedSignal + "\" listed in the signals array?)" +
                     __mismatchValuesFormat(signalName, expectedSignal))
        sequenceFailure = true
    }

    function __mismatchValuesFormat(actual, expected) {
        return "\n    Actual   : " + actual +
               "\n    Expected : " + expected +
               "\n    Sequence index: " + sequenceIndex
    }
}
