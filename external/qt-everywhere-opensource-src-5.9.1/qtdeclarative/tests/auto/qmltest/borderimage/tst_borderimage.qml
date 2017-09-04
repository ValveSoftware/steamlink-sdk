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
import QtTest 1.1

Item {
    id: top
    property bool canconnect
    property bool checkfinished: false

    Component.onCompleted: {
        var check = new XMLHttpRequest;
        check.open("GET", "http://127.0.0.1:14445/colors.png");
        check.onreadystatechange = function() {
            if (check.readyState == XMLHttpRequest.DONE) {
                if (check.status == 404) {
                    top.canconnect = false;
                }else{
                    top.canconnect = true;
                }
                top.checkfinished = true;
            }
        }
        check.send();
    }

    BorderImage {
        id: noSource
        source: ""
    }

    property string srcImage: "colors.png"

    BorderImage {
        id: clearSource
        source: srcImage
    }

    BorderImage {
        id: resized
        source: srcImage
        width: 300
        height: 300
    }

    BorderImage {
        id: smooth
        source: srcImage
        smooth: true
        width: 300
        height: 300
    }

    BorderImage {
        id: tileModes1
        source: srcImage
        width: 100
        height: 300
        horizontalTileMode: BorderImage.Repeat
        verticalTileMode: BorderImage.Repeat
    }

    BorderImage {
        id: tileModes2
        source: srcImage
        width: 300
        height: 150
        horizontalTileMode: BorderImage.Round
        verticalTileMode: BorderImage.Round
    }

    TestCase {
        name: "BorderImage"

        function test_noSource() {
            compare(noSource.source, "")
            compare(noSource.width, 0)
            compare(noSource.height, 0)
            compare(noSource.horizontalTileMode, BorderImage.Stretch)
            compare(noSource.verticalTileMode, BorderImage.Stretch)
        }

        function test_imageSource_data() {
            return [
                {
                    tag: "local",
                    source: "colors.png",
                    remote: false,
                    error: ""
                },
                {
                    tag: "local not found",
                    source: "no-such-file.png",
                    remote: false,
                    error: "SUBinline:1:21: QML BorderImage: Cannot open: SUBno-such-file.png"
                },
                {
                    tag: "remote",
                    source: "http://127.0.0.1:14445/colors.png",
                    remote: true,
                    error: ""
                }
            ]
        }

        function test_imageSource(row) {
            var expectError = (row.error.length != 0)
            if (expectError) {
                var parentUrl = Qt.resolvedUrl(".")
                ignoreWarning(row.error.replace(/SUB/g, parentUrl))
            }

            var img = Qt.createQmlObject
                ('import QtQuick 2.0; BorderImage { source: "' +
                    row.source + '" }', top)

            if (row.remote) {
                skip("Remote solution not yet complete")
                tryCompare(img, "status", BorderImage.Loading)
                tryCompare(top, "checkfinished", true, 10000)
                if (top.canconnect == false)
                    skip("Cannot access remote")
            }

            if (!expectError) {
                tryCompare(img, "status", BorderImage.Ready, 10000)
                compare(img.width, 120)
                compare(img.height, 120)
                compare(img.horizontalTileMode, BorderImage.Stretch)
                compare(img.verticalTileMode, BorderImage.Stretch)
            } else {
                tryCompare(img, "status", BorderImage.Error)
            }

            img.destroy()
        }

        function test_clearSource() {
            compare(clearSource.source, Qt.resolvedUrl("colors.png"))
            compare(clearSource.width, 120)
            compare(clearSource.height, 120)

            srcImage = ""
            compare(clearSource.source, "")
            compare(clearSource.width, 0)
            compare(clearSource.height, 0)
        }

        function test_resized() {
            compare(resized.width, 300)
            compare(resized.height, 300)
            compare(resized.horizontalTileMode, BorderImage.Stretch)
            compare(resized.verticalTileMode, BorderImage.Stretch)
        }

        function test_smooth() {
            compare(smooth.smooth, true)
            compare(smooth.width, 300)
            compare(smooth.height, 300)
            compare(smooth.horizontalTileMode, BorderImage.Stretch)
            compare(smooth.verticalTileMode, BorderImage.Stretch)
        }

        function test_tileModes() {
            compare(tileModes1.width, 100)
            compare(tileModes1.height, 300)
            compare(tileModes1.horizontalTileMode, BorderImage.Repeat)
            compare(tileModes1.verticalTileMode, BorderImage.Repeat)

            compare(tileModes2.width, 300)
            compare(tileModes2.height, 150)
            compare(tileModes2.horizontalTileMode, BorderImage.Round)
            compare(tileModes2.verticalTileMode, BorderImage.Round)
        }

        function test_sciSource_data() {
            return [
                {
                    tag: "local",
                    source: "colors-round.sci",
                    remote: false,
                    valid: true
                },
                {
                    tag: "local not found",
                    source: "no-such-file.sci",
                    remote: false,
                    valid: false
                },
                {
                    tag: "remote",
                    source: "remote.sci",
                    remote: true,
                    valid: true
                }
            ]
        }

        function test_sciSource(row) {
            var img = Qt.createQmlObject('import QtQuick 2.0; BorderImage { height: 300; width: 300 }', top)

            if (row.remote) {
                skip("Remote solution not yet complete")
                img.source = row.source;
                tryCompare(top, "checkfinished", true, 10000)
                if (top.canconnect == false)
                    skip("Cannot access remote")
            }else{
                img.source = row.source;
            }

            compare(img.source, Qt.resolvedUrl(row.source))
            compare(img.width, 300)
            compare(img.height, 300)

            if (row.valid) {
                tryCompare(img, "status", BorderImage.Ready, 10000)
                compare(img.border.left, 10)
                compare(img.border.top, 20)
                compare(img.border.right, 30)
                compare(img.border.bottom, 40)
                compare(img.horizontalTileMode, BorderImage.Round)
                compare(img.verticalTileMode, BorderImage.Repeat)
            } else {
                tryCompare(img, "status", BorderImage.Error)
            }

            img.destroy()
        }


        function test_invalidSciFile() {
            ignoreWarning("QQuickGridScaledImage: Invalid tile rule specified. Using Stretch.") // for "Roun"
            ignoreWarning("QQuickGridScaledImage: Invalid tile rule specified. Using Stretch.") // for "Repea"

            var component = Qt.createComponent("InvalidSciFile.qml")
            var invalidSciFile = component.createObject(top)

            compare(invalidSciFile.status, Image.Error)
            compare(invalidSciFile.width, 300)
            compare(invalidSciFile.height, 300)
            compare(invalidSciFile.horizontalTileMode, BorderImage.Stretch)
            compare(invalidSciFile.verticalTileMode, BorderImage.Stretch)
        }
    }
}
