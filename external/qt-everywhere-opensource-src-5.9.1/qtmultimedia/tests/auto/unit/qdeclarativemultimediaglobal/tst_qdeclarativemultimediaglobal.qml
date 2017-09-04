/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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
import QtMultimedia 5.4

TestCase {

    function test_0_globalObject() {
        verify(typeof QtMultimedia !== 'undefined');
    }

    function test_1_defaultCamera() {
        verify(typeof QtMultimedia.defaultCamera !== 'undefined');

        var camera = QtMultimedia.defaultCamera;
        compare(camera.deviceId, "othercamera", "deviceId");
        compare(camera.displayName, "othercamera desc", "displayName");
        compare(camera.position, Camera.UnspecifiedPosition, "position");
        compare(camera.orientation, 0, "orientation");
    }

    function test_2_availableCameras() {
        verify(typeof QtMultimedia.availableCameras !== 'undefined');
        compare(QtMultimedia.availableCameras.length, 2);

        var camera = QtMultimedia.availableCameras[0];
        compare(camera.deviceId, "backcamera", "deviceId");
        compare(camera.displayName, "backcamera desc", "displayName");
        compare(camera.position, Camera.BackFace, "position");
        compare(camera.orientation, 90, "orientation");

        camera = QtMultimedia.availableCameras[1];
        compare(camera.deviceId, "othercamera", "deviceId");
        compare(camera.displayName, "othercamera desc", "displayName");
        compare(camera.position, Camera.UnspecifiedPosition, "position");
        compare(camera.orientation, 0, "orientation");
    }

    function test_convertVolume_data() {
        return [
            { tag: "-1.0 from linear to linear", input: -1, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from linear to linear", input: 0, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0 },
            { tag: "0.5 from linear to linear", input: 0.5, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.5 },
            { tag: "1.0 from linear to linear", input: 1.0, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 1.0 },

            { tag: "-1.0 from linear to cubic", input: -1, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from linear to cubic", input: 0, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0 },
            { tag: "0.33 from linear to cubic", input: 0.33, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.69 },
            { tag: "0.5 from linear to cubic", input: 0.5, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.79 },
            { tag: "0.72 from linear to cubic", input: 0.72, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.89 },
            { tag: "1.0 from linear to cubic", input: 1.0, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 1.0 },

            { tag: "-1.0 from linear to logarithmic", input: -1, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from linear to logarithmic", input: 0, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0 },
            { tag: "0.33 from linear to logarithmic", input: 0.33, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.78 },
            { tag: "0.5 from linear to logarithmic", input: 0.5, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.9 },
            { tag: "0.72 from linear to logarithmic", input: 0.72, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.96 },
            { tag: "1.0 from linear to logarithmic", input: 1.0, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 1.0 },

            { tag: "-1.0 from linear to decibel", input: -1, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -200 },
            { tag: "0.0 from linear to decibel", input: 0, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -200 },
            { tag: "0.33 from linear to decibel", input: 0.33, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -9.63 },
            { tag: "0.5 from linear to decibel", input: 0.5, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -6.02 },
            { tag: "0.72 from linear to decibel", input: 0.72, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -2.85 },
            { tag: "1.0 from linear to decibel", input: 1.0, from: QtMultimedia.LinearVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: 0 },

            { tag: "-1.0 from cubic to linear", input: -1, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from cubic to linear", input: 0, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0 },
            { tag: "0.33 from cubic to linear", input: 0.33, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.04 },
            { tag: "0.5 from cubic to linear", input: 0.5, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.13 },
            { tag: "0.72 from cubic to linear", input: 0.72, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.37 },
            { tag: "1.0 from cubic to linear", input: 1.0, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 1 },

            { tag: "-1.0 from cubic to cubic", input: -1, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from cubic to cubic", input: 0, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0 },
            { tag: "0.5 from cubic to cubic", input: 0.5, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.5 },
            { tag: "1.0 from cubic to cubic", input: 1.0, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 1.0 },

            { tag: "-1.0 from cubic to logarithmic", input: -1, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from cubic to logarithmic", input: 0, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0 },
            { tag: "0.33 from cubic to logarithmic", input: 0.33, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.15 },
            { tag: "0.5 from cubic to logarithmic", input: 0.5, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.44 },
            { tag: "0.72 from cubic to logarithmic", input: 0.72, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.82 },
            { tag: "1.0 from cubic to logarithmic", input: 1.0, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 1 },

            { tag: "-1.0 from cubic to decibel", input: -1, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -200 },
            { tag: "0.0 from cubic to decibel", input: 0, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -200 },
            { tag: "0.33 from cubic to decibel", input: 0.33, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -28.89 },
            { tag: "0.5 from cubic to decibel", input: 0.5, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -18.06 },
            { tag: "0.72 from cubic to decibel", input: 0.72, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -8.56 },
            { tag: "1.0 from cubic to decibel", input: 1.0, from: QtMultimedia.CubicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: 0 },

            { tag: "-1.0 from logarithmic to linear", input: -1, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from logarithmic to linear", input: 0, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0 },
            { tag: "0.33 from logarithmic to linear", input: 0.33, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.09 },
            { tag: "0.5 from logarithmic to linear", input: 0.5, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.15 },
            { tag: "0.72 from logarithmic to linear", input: 0.72, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.28 },
            { tag: "1.0 from logarithmic to linear", input: 1.0, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 1.0 },

            { tag: "-1.0 from logarithmic to cubic", input: -1, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from logarithmic to cubic", input: 0, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0 },
            { tag: "0.33 from logarithmic to cubic", input: 0.33, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.44 },
            { tag: "0.5 from logarithmic to cubic", input: 0.5, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.53 },
            { tag: "0.72 from logarithmic to cubic", input: 0.72, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.65 },
            { tag: "1.0 from logarithmic to cubic", input: 1.0, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 1.0 },

            { tag: "-1.0 from logarithmic to logarithmic", input: -1, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0 },
            { tag: "0.0 from logarithmic to logarithmic", input: 0, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0 },
            { tag: "0.5 from logarithmic to logarithmic", input: 0.5, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.5 },
            { tag: "1.0 from logarithmic to logarithmic", input: 1.0, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 1.0 },

            { tag: "-1.0 from logarithmic to decibel", input: -1, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -200 },
            { tag: "0.0 from logarithmic to decibel", input: 0, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -200 },
            { tag: "0.33 from logarithmic to decibel", input: 0.33, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -21.21 },
            { tag: "0.5 from logarithmic to decibel", input: 0.5, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -16.45 },
            { tag: "0.72 from logarithmic to decibel", input: 0.72, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -11.17 },
            { tag: "1.0 from logarithmic to decibel", input: 1.0, from: QtMultimedia.LogarithmicVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: 0 },

            { tag: "-1000 from decibel to linear", input: -1000, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0 },
            { tag: "-200 from decibel to linear", input: -200, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0 },
            { tag: "-40 from decibel to linear", input: -40, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.01 },
            { tag: "-10 from decibel to linear", input: -10, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.32 },
            { tag: "-5 from decibel to linear", input: -5, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 0.56 },
            { tag: "0 from decibel to linear", input: 0, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LinearVolumeScale, expectedOutput: 1 },

            { tag: "-1000 from decibel to cubic", input: -1000, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0 },
            { tag: "-200 from decibel to cubic", input: -200, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0 },
            { tag: "-40 from decibel to cubic", input: -40, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.22 },
            { tag: "-10 from decibel to cubic", input: -10, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.68 },
            { tag: "-5 from decibel to cubic", input: -5, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 0.83 },
            { tag: "0 from decibel to cubic", input: 0, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.CubicVolumeScale, expectedOutput: 1 },

            { tag: "-1000 from decibel to logarithmic", input: -1000, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0 },
            { tag: "-200 from decibel to logarithmic", input: -200, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0 },
            { tag: "-40 from decibel to logarithmic", input: -40, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.05 },
            { tag: "-10 from decibel to logarithmic", input: -10, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.77 },
            { tag: "-5 from decibel to logarithmic", input: -5, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 0.92 },
            { tag: "0 from decibel to logarithmic", input: 0, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.LogarithmicVolumeScale, expectedOutput: 1 },

            { tag: "-1000 from decibel to decibel", input: -1000, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -1000 },
            { tag: "-200 from decibel to decibel", input: -200, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -200 },
            { tag: "-30 from decibel to decibel", input: -30, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: -30 },
            { tag: "0 from decibel to decibel", input: 0, from: QtMultimedia.DecibelVolumeScale, to: QtMultimedia.DecibelVolumeScale, expectedOutput: 0 },
        ]
    }

    function test_convertVolume(data) {
        fuzzyCompare(QtMultimedia.convertVolume(data.input, data.from, data.to),
                     data.expectedOutput,
                     0.01);
    }

}
