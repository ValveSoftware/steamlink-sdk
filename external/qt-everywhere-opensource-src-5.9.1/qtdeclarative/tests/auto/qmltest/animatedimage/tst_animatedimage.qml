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

import QtQuick 2.0
import QtTest 1.1

Item {
    id: top
    property string srcImage: "stickman.gif"
    property bool canconnect
    property bool checkfinished: false

    Component.onCompleted: {
        var check = new XMLHttpRequest;
        check.open("GET", "http://127.0.0.1:14445/stickman.gif");
        check.onreadystatechange = function() {

            console.log("Status: ", check.status)
            console.log("Readystate", check.readyState)
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

    AnimatedImage {
        id: noSource
        source: ""
    }

    AnimatedImage {
        id: clearSource
        source: srcImage
    }

    AnimatedImage {
        id: resized
        source: srcImage
        width: 300
        height: 300
    }

    AnimatedImage {
        id: smooth
        source: srcImage
        smooth: true
        width: 300
        height: 300
    }

    AnimatedImage {
        id: tileModes1
        source: srcImage
        width: 100
        height: 300
        fillMode: AnimatedImage.Tile
    }

    AnimatedImage {
        id: tileModes2
        source: srcImage
        width: 300
        height: 150
        fillMode: AnimatedImage.TileVertically
    }
    AnimatedImage {
        id: tileModes3
        source: srcImage
        width: 300
        height: 150
        fillMode: AnimatedImage.TileHorizontally
    }

    TestCase {
        name: "AnimatedImage"

        function test_noSource() {
            compare(noSource.source, "")
            compare(noSource.width, 0)
            compare(noSource.height, 0)
            compare(noSource.fillMode, AnimatedImage.Stretch)
        }

        function test_imageSource_data() {
            return [
                {
                    tag: "local",
                    source: "stickman.gif",
                    remote: false,
                    error: ""
                },
                {
                    tag: "local not found",
                    source: "no-such-file.png",
                    remote: false,
                    error: "SUBinline:1:21: QML AnimatedImage: Error Reading Animated Image File SUBno-such-file.png"
                },
                {
                    tag: "remote",
                    source: "http://127.0.0.1:14445/stickman.gif",
                    remote: true,
                    error: ""
                }
            ]
        }

        function test_imageSource(row) {
            var expectError = (row.error.length != 0)
            var canconnect = false;

            if (expectError) {
                var parentUrl = Qt.resolvedUrl(".")
                ignoreWarning(row.error.replace(/SUB/g, parentUrl))
            }

            var img = Qt.createQmlObject('import QtQuick 2.0; AnimatedImage { source: "'+row.source+'" }', top)

            if (row.remote) {
                skip("Remote solution not yet complete")
                tryCompare(img, "status", AnimatedImage.Loading)
                tryCompare(top, "checkfinished", true, 10000)
                if (top.canconnect == false)
                    skip("Cannot access remote")
            }

            if (!expectError) {
                tryCompare(img, "status", AnimatedImage.Ready, 10000)
                compare(img.width, 160)
                compare(img.height, 120)
                compare(img.fillMode, AnimatedImage.Stretch)
            } else {
                tryCompare(img, "status", AnimatedImage.Error)
            }

            img.destroy()
        }

        function test_clearSource() {
            compare(clearSource.source, Qt.resolvedUrl(srcImage))
            compare(clearSource.width, 160)
            compare(clearSource.height, 120)

            srcImage = ""
            compare(clearSource.source, "")
            compare(clearSource.width, 0)
            compare(clearSource.height, 0)
        }

        function test_resized() {
            compare(resized.width, 300)
            compare(resized.height, 300)
            compare(resized.fillMode, AnimatedImage.Stretch)
        }

        function test_smooth() {
            compare(smooth.smooth, true)
            compare(smooth.width, 300)
            compare(smooth.height, 300)
            compare(smooth.fillMode, AnimatedImage.Stretch)
        }

        function test_tileModes() {
            compare(tileModes1.width, 100)
            compare(tileModes1.height, 300)
            compare(tileModes1.fillMode, AnimatedImage.Tile)

            compare(tileModes2.width, 300)
            compare(tileModes2.height, 150)
            compare(tileModes2.fillMode, AnimatedImage.TileVertically)

            compare(tileModes3.width, 300)
            compare(tileModes3.height, 150)
            compare(tileModes3.fillMode, AnimatedImage.TileHorizontally)
        }

    }
}
