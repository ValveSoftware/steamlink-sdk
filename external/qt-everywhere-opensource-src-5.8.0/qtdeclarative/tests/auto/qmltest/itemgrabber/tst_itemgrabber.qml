/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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

import QtQuick 2.4
import QtTest 1.1

Item {
    id: root;
    width: 400
    height: 400

    TestCase {
        id: testCase
        name: "item-grabber"
        when: imageOnDisk.ready && imageOnDiskSmall.ready && imageInCache.ready && imageInCacheSmall.ready
        function test_endresult() {
            var image = grabImage(root);

            // imageOnDisk at (0, 0) - (100x100)
            compare(imageOnDisk.width, 100);
            compare(imageOnDisk.height, 100);
            verify(image.pixel(0, 0) === Qt.rgba(1, 0, 0, 1)); // Use verify because compare doesn't support colors (QTBUG-34878)
            verify(image.pixel(99, 99) === Qt.rgba(0, 0, 1, 1));

            // imageOnDiskSmall at (100, 0) - 50x50
            compare(imageOnDiskSmall.width, 50);
            compare(imageOnDiskSmall.height, 50);
            verify(image.pixel(100, 0) === Qt.rgba(1, 0, 0, 1));
            verify(image.pixel(149, 49) === Qt.rgba(0, 0, 1, 1));

            // imageInCache at (0, 100) - 100x100
            compare(imageInCache.width, 100);
            compare(imageInCache.height, 100);
            verify(image.pixel(0, 100) === Qt.rgba(1, 0, 0, 1));
            verify(image.pixel(99, 199) === Qt.rgba(0, 0, 1, 1));

            // imageInCacheSmall at (100, 100) - 50x50
            compare(imageInCacheSmall.width, 50);
            compare(imageInCacheSmall.height, 50);
            verify(image.pixel(100, 100) === Qt.rgba(1, 0, 0, 1));
            verify(image.pixel(149, 149) === Qt.rgba(0, 0, 1, 1));

            // After all that has been going on, it should only have been called that one time..
            compare(imageOnDisk.callCount, 1);
        }

        onWindowShownChanged: {
            box.grabToImage(imageOnDisk.handleGrab);
            box.grabToImage(imageOnDiskSmall.handleGrab, Qt.size(50, 50));
            box.grabToImage(imageInCache.handleGrab);
            box.grabToImage(imageInCacheSmall.handleGrab, Qt.size(50, 50));
        }

    }

    Rectangle {
        id: box
        width: 100
        height: 100
        color: "red";

        visible: false

        Rectangle {
            anchors.bottom: parent.bottom;
            anchors.right: parent.right;
            width: 10
            height: 10
            color: "blue";
        }
    }

    Image {
        id: imageOnDisk
        x: 0
        y: 0
        property int callCount: 0;
        property bool ready: false;
        function handleGrab(result) {
            if (!result.saveToFile("itemgrabber/image.png"))
                print("Error: Failed to save image to disk...");
            source = "image.png";
            ready = true;
            ++callCount;
        }
    }

    Image {
        id: imageOnDiskSmall
        x: 100
        y: 0
        property bool ready: false;
        function handleGrab(result) {
            if (!result.saveToFile("itemgrabber/image_small.png"))
                print("Error: Failed to save image to disk...");
            source = "image_small.png";
            ready = true;
        }
    }

    Image {
        id: imageInCache
        x: 0
        y: 100
        property bool ready: false;
        function handleGrab(result) {
            source = result.url;
            ready = true;
        }
    }

    Image {
        id: imageInCacheSmall
        x: 100
        y: 100
        property bool ready: false;
        function handleGrab(result) {
            source = result.url;
            ready = true;
        }
    }
}
