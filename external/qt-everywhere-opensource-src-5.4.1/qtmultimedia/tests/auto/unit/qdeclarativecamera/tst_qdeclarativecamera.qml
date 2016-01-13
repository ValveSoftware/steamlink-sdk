/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
}
