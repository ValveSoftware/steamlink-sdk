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

    Component {
        id: cameraComponent
        Camera { }
    }

    Loader {
        id: cameraLoader
    }

    SignalSpy {
        id: deviceIdSpy
        target: cameraLoader.item
        signalName: "deviceIdChanged"
    }

    function test_deviceId() {
        deviceIdSpy.clear();
        cameraLoader.sourceComponent = cameraComponent;
        var camera = cameraLoader.item;

        // default camera
        compare(camera.deviceId, "othercamera", "deviceId");
        compare(camera.displayName, "othercamera desc", "displayName");
        compare(camera.position, Camera.UnspecifiedPosition, "position");
        compare(camera.orientation, 0, "orientation");

        // setting an invalid camera ID should leave the item unchanged
        camera.deviceId = "invalidcamera";
        compare(camera.deviceId, "othercamera");
        compare(deviceIdSpy.count, 0);
        compare(camera.displayName, "othercamera desc", "displayName");
        compare(camera.position, Camera.UnspecifiedPosition, "position");
        compare(camera.orientation, 0, "orientation");

        // change to another valid camera
        camera.deviceId = "backcamera";
        compare(camera.deviceId, "backcamera");
        compare(deviceIdSpy.count, 1);
        compare(camera.displayName, "backcamera desc", "displayName");
        compare(camera.position, Camera.BackFace, "position");
        compare(camera.orientation, 90, "orientation");

        // setting an empty device ID should load the default camera
        camera.deviceId = "";
        compare(camera.deviceId, "othercamera", "deviceId");
        compare(deviceIdSpy.count, 2);

        cameraLoader.sourceComponent = undefined;
    }

    function test_position() {
        deviceIdSpy.clear();
        cameraLoader.sourceComponent = cameraComponent;
        var camera = cameraLoader.item;

        // default camera
        compare(camera.position, Camera.UnspecifiedPosition, "position");
        compare(camera.deviceId, "othercamera", "deviceId");

        // setting an unavailable camera position should leave the item unchanged
        camera.position = Camera.FrontFace;
        compare(camera.position, Camera.UnspecifiedPosition, "position");
        compare(camera.deviceId, "othercamera", "deviceId");
        compare(deviceIdSpy.count, 0);

        // change to an available position
        camera.position = Camera.BackFace;
        compare(camera.position, Camera.BackFace, "position");
        compare(camera.deviceId, "backcamera", "deviceId");
        compare(deviceIdSpy.count, 1);

        // setting UnspecifiedPosition should load the default camera
        camera.position = Camera.UnspecifiedPosition;
        compare(camera.position, Camera.UnspecifiedPosition, "position");
        compare(camera.deviceId, "othercamera", "deviceId");
        compare(deviceIdSpy.count, 2);

        cameraLoader.sourceComponent = undefined;
    }

    SignalSpy {
        id: cameraStateSpy
        target: cameraLoader.item
        signalName: "cameraStateChanged"
    }

    function test_cameraState() {
        deviceIdSpy.clear();
        cameraStateSpy.clear();
        cameraLoader.sourceComponent = cameraComponent;
        var camera = cameraLoader.item;

        // camera should be in ActiveState by default
        compare(camera.cameraState, Camera.ActiveState, "cameraState");
        compare(camera.deviceId, "othercamera", "deviceId");

        // Changing the camera device should unload the previous camera and apply the current state
        // to the new camera
        camera.deviceId = "backcamera";
        compare(camera.deviceId, "backcamera", "deviceId");
        compare(camera.cameraState, Camera.ActiveState, "cameraState");
        compare(cameraStateSpy.count, 2);
        compare(cameraStateSpy.signalArguments[0][0], Camera.UnloadedState);
        compare(cameraStateSpy.signalArguments[1][0], Camera.ActiveState);

        cameraLoader.sourceComponent = undefined;
    }

    function test_supportedViewfinderResolutions_data() {
        // see mockcameraviewfindersettingscontrol.h for expected values

        return [
            {
                tag: "all",
                minimumFrameRate: 0, maximumFrameRate: 0,
                expectedResolutions: [
                    { width: 320, height: 240 },
                    { width: 640, height: 480 },
                    { width: 1280, height: 720 },
                    { width: 1920, height: 1080 }
                ]
            },
            {
                tag: "invalid minimumFrameRate",
                minimumFrameRate: 2, maximumFrameRate: 0,
                expectedResolutions: [ ]
            },
            {
                tag: "minimumFrameRate=5",
                minimumFrameRate: 5, maximumFrameRate: 0,
                expectedResolutions: [
                    { width: 1920, height: 1080 }
                ]
            },
            {
                tag: "minimumFrameRate=10",
                minimumFrameRate: 10, maximumFrameRate: 0,
                expectedResolutions: [
                    { width: 1280, height: 720 }
                ]
            },
            {
                tag: "minimumFrameRate=30",
                minimumFrameRate: 30, maximumFrameRate: 0,
                expectedResolutions: [
                    { width: 320, height: 240 },
                    { width: 640, height: 480 },
                    { width: 1280, height: 720 }
                ]
            },
            {
                tag: "invalid maximumFrameRate",
                minimumFrameRate: 0, maximumFrameRate: 2,
                expectedResolutions: [ ]
            },
            {
                tag: "maximumFrameRate=10",
                minimumFrameRate: 0, maximumFrameRate: 10,
                expectedResolutions: [
                    { width: 1280, height: 720 },
                    { width: 1920, height: 1080 }
                ]
            },
            {
                tag: "minimumFrameRate=10, maximumFrameRate=10",
                minimumFrameRate: 10, maximumFrameRate: 10,
                expectedResolutions: [
                    { width: 1280, height: 720 }
                ]
            },
            {
                tag: "minimumFrameRate=30, maximumFrameRate=30",
                minimumFrameRate: 30, maximumFrameRate: 30,
                expectedResolutions: [
                    { width: 320, height: 240 },
                    { width: 640, height: 480 },
                    { width: 1280, height: 720 }
                ]
            }
        ]
    }

    function test_supportedViewfinderResolutions(data) {
        cameraLoader.sourceComponent = cameraComponent;
        var camera = cameraLoader.item;

        var actualResolutions = camera.supportedViewfinderResolutions(data.minimumFrameRate, data.maximumFrameRate);
        compare(actualResolutions.length, data.expectedResolutions.length);
        for (var i = 0; i < actualResolutions.length; ++i) {
            compare(actualResolutions[i].width, data.expectedResolutions[i].width);
            compare(actualResolutions[i].height, data.expectedResolutions[i].height);
        }

        cameraLoader.sourceComponent = undefined;
    }

    function test_supportedViewfinderFrameRateRanges_data() {
        // see mockcameraviewfindersettingscontrol.h for expected values
        return [
            {
                tag: "all",
                expectedFrameRateRanges: [
                    { minimumFrameRate: 5, maximumFrameRate: 10 },
                    { minimumFrameRate: 10, maximumFrameRate: 10 },
                    { minimumFrameRate: 30, maximumFrameRate: 30 }
                ]
            },
            {
                tag: "invalid",
                resolution: { width: 452472, height: 444534 },
                expectedFrameRateRanges: [ ]
            },
            {
                tag: "320, 240",
                resolution: { width: 320, height: 240 },
                expectedFrameRateRanges: [
                    { minimumFrameRate: 30, maximumFrameRate: 30 }
                ]
            },
            {
                tag: "1280, 720",
                resolution: { width: 1280, height: 720 },
                expectedFrameRateRanges: [
                    { minimumFrameRate: 10, maximumFrameRate: 10 },
                    { minimumFrameRate: 30, maximumFrameRate: 30 }
                ]
            },
            {
                tag: "1920, 1080",
                resolution: { width: 1920, height: 1080 },
                expectedFrameRateRanges: [
                    { minimumFrameRate: 5, maximumFrameRate: 10 }
                ]
            }
        ]
    }

    function test_supportedViewfinderFrameRateRanges(data) {
        cameraLoader.sourceComponent = cameraComponent;
        var camera = cameraLoader.item;

        // Pass the resolution as an object
        var actualFrameRateRanges = camera.supportedViewfinderFrameRateRanges(data.resolution);
        compare(actualFrameRateRanges.length, data.expectedFrameRateRanges.length);
        for (var i = 0; i < actualFrameRateRanges.length; ++i) {
            compare(actualFrameRateRanges[i].minimumFrameRate, data.expectedFrameRateRanges[i].minimumFrameRate);
            compare(actualFrameRateRanges[i].maximumFrameRate, data.expectedFrameRateRanges[i].maximumFrameRate);
        }

        // Pass the resolution as a size
        if (typeof data.resolution !== 'undefined') {
            actualFrameRateRanges = camera.supportedViewfinderFrameRateRanges(Qt.size(data.resolution.width, data.resolution.height));
            compare(actualFrameRateRanges.length, data.expectedFrameRateRanges.length);
            for (i = 0; i < actualFrameRateRanges.length; ++i) {
                compare(actualFrameRateRanges[i].minimumFrameRate, data.expectedFrameRateRanges[i].minimumFrameRate);
                compare(actualFrameRateRanges[i].maximumFrameRate, data.expectedFrameRateRanges[i].maximumFrameRate);
            }
        }

        cameraLoader.sourceComponent = undefined;
    }
}
