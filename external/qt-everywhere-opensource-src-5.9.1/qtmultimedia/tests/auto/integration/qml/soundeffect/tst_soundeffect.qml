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
import QtMultimedia 5.0
import QtTest 1.0

/*
    Component {
        name: "QSoundEffect"
        prototype: "QObject"
        exports: ["SoundEffect 5.0"]
        Enum {
            name: "Loop"
            values: {
                "Infinite": -2
            }
        }
        Enum {
            name: "Status"
            values: {
                "Null": 0,
                "Loading": 1,
                "Ready": 2,
                "Error": 3
            }
        }
        Property { name: "source"; type: "QUrl" }
        Property { name: "loops"; type: "int" }
        Property { name: "loopsRemaining"; type: "int"; isReadonly: true }
        Property { name: "volume"; type: "double" }
        Property { name: "muted"; type: "bool" }
        Property { name: "playing"; type: "bool"; isReadonly: true }
        Property { name: "status"; type: "Status"; isReadonly: true }
        Property { name: "category"; type: "string" }
        Signal { name: "sourceChanged"; type: "void" }
        Signal { name: "loopCountChanged"; type: "void" }
        Signal { name: "loopsRemainingChanged"; type: "void" }
        Signal { name: "volumeChanged"; type: "void" }
        Signal { name: "mutedChanged"; type: "void" }
        Signal { name: "loadedChanged"; type: "void" }
        Signal { name: "playingChanged"; type: "void" }
        Signal { name: "statusChanged"; type: "void" }
        Signal { name: "categoryChanged"; type: "void" }
        Method { name: "play"; type: "void" }
        Method { name: "stop"; type: "void" }
    }
*/

Item {
    id: top

    property string srcWav: "../../../integration/qsoundeffect/test.wav"

    SoundEffect {
        id: sound1
        source: srcWav
    }

    SignalSpy {
        id: spySource
        target: sound1
        signalName: "sourceChanged"
    }

    SignalSpy {
        id: spyLoop
        target: sound1
        signalName: "loopCountChanged"
    }

    SignalSpy {
        id: spyRemaining
        target: sound1
        signalName: "loopsRemainingChanged"
    }

    SignalSpy {
        id: spyVolume
        target: sound1
        signalName: "volumeChanged"
    }

    SignalSpy {
        id: spyMuted
        target: sound1
        signalName: "mutedChanged"
    }

    SignalSpy {
        id: spyLoaded
        target: sound1
        signalName: "loadedChanged"
    }

    TestCase {
        name: "SoundEffects"

        function initTestCase() {
            // Check initial properties
            verify(sound1.loops == 1)
            verify(sound1.volume == 1.0)
            verify(sound1.loopsRemaining == 0)
            verify(sound1.muted == false)
            verify(sound1.playing == false)
            verify(sound1.status == 1) // Status.Loading
        }

        function test_muting() {
            compare(spyMuted.count, 0)
            sound1.muted = true
            compare(sound1.muted, true)
            compare(spyMuted.count, 1)

            sound1.muted = false
            compare(sound1.muted, false)
            compare(spyMuted.count, 2)
        }

        function test_looping() {
            spyLoop.clear
            sound1.loops = 2
            compare(sound1.loops, 2)
            compare(spyLoop.count, 1)
            sound1.loops = 1
            compare(sound1.loops, 1)
        }

        function test_source() {
            spyLoaded.clear
            spySource.clear
            sound1.source = "../../../integration/qsoundeffect/test.wav"
            compare(spySource.count, 1)
            tryCompare(spyLoaded, "count", 1, 3000)
            tryCompare(spySource, "count", 1, 3000)
        }

        function test_playSound() {
            spyRemaining.clear
            sound1.source = srcWav
            sound1.loops = 0
            sound1.play()
            tryCompare(sound1, "playing", false)
            compare(sound1.loopsRemaining, 0)
        }

        function test_loopsRemaining()
        {
            spyRemaining.clear
            sound1.loops = 3
            compare(sound1.loops, 3)
            sound1.play()
            tryCompare(sound1, "playing", false)
            compare(spyRemaining.count, 4)
            compare(sound1.loopsRemaining, 0)
        }

        function test_volume()
        {
            spyVolume.clear
            sound1.volume = 0.5
            compare(sound1.volume, 0.5)
            sound1.volume = 0
            compare(sound1.volume, 0)
            sound1.volume = 1
            compare(spyVolume.count, 3)
        }
    }
}
