/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.2
import QtTest 1.0
import QtQuick.Layouts 1.1

Item {
    id: container
    width: 200
    height: 200
    TestCase {
        id: testCase
        name: "Tests_Baselines"
        when: windowShown
        width: 200
        height: 200

        function test_baselineOffset_data()
        {
            return [
                { tag: "Text",   controlSpec: "Text { text: \"LLLLLLLLLLLLLLL\"}"},
                { tag: "Button", controlSpec: "Button { text: \"LLLLLLLLLLLLLLL\"}"},
                //{ tag: "SpinBox", controlSpec: "SpinBox { implicitWidth: 80; prefix:\"LLLLLLLLLLLLLL\"; value: 2}"},
                //{ tag: "CheckBox", controlSpec: "CheckBox { text: \"LLLLLLLLLLLLLL\" }"},
            ];
        }

        function test_baselineOffset(data)
        {
            skip("Started to fail on Ubuntu 10.04 around 22/07/2015. To be fixed as soon as we can check on the VM.")
            var item = Qt.createQmlObject('import QtQuick 2.1;import QtQuick.Controls 1.1;' + data.controlSpec,
                                          container, '')
            waitForRendering(item)
            var img = grabImage(item);

            // the image object returned by grabImage does not have width and height properties,
            // but its dimensions should be the same as the item it was grabbed from
            var imgWidth = item.width
            var imgHeight = item.height

            var lineIntensities = []
            var lineSum = 0;
            for (var y = 0; y < imgHeight; ++y) {
                var lineIntensity = 0
                for (var x = 0; x < imgWidth; ++x)
                    lineIntensity += img.red(x,y) + img.green(x,y) + img.blue(x,y)

                lineIntensities.push(lineIntensity)
                lineSum += lineIntensity
            }
            var lineAverage = lineSum / imgHeight;
            // now that we have the whole controls _average_ intensity, we assume that the lineIntensity
            // with the biggest difference in intensity from the lineAverage is where the baseline is.
            var visualBaselineOffset = -1
            var maxDifferenceFromAverage = 0;
            for (var y = 0; y < lineIntensities.length; ++y) {
                var differenceFromAverage = Math.abs(lineAverage - lineIntensities[y])
                if (differenceFromAverage > maxDifferenceFromAverage) {
                    maxDifferenceFromAverage = differenceFromAverage
                    visualBaselineOffset = y;
                }
            }

            // Allow that the visual baseline is one off
            // This is mainly because baselines of text are actually one pixel *below* the text baseline
            // In addition it is an acceptable error
            var threshold = (Qt.platform.os === "osx" ? 2 : 1)      // its only 2 on osx 10.7, but we cannot determine the os version from here.
            fuzzyCompare(visualBaselineOffset, item.baselineOffset, threshold)

            item.destroy();
        }

    }
}
