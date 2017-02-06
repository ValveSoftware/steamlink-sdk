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

import QtTest 1.0
import QtQuick 2.1
import "TestUtils.js" as TestUtils

TestCase {
    id: testcase
    name: "Tests_CircularTickmarkLabel"
    visible: true
    when: windowShown
    width: 400
    height: 400

    property var label: null

    function init() {
        label = Qt.createQmlObject("import QtQuick.Extras 1.4; import QtQuick.Extras.Private 1.0; CircularTickmarkLabel {}", testcase, "");
        verify(label, "CircularTickmarkLabel: failed to create an instance");
        verify(label.__style);
    }

    function cleanup() {
        label.destroy();
    }

    function test_angleRange() {
        label.minimumValueAngle = -180;
        label.maximumValueAngle = 180;
        compare(label.angleRange, 360);

        label.minimumValueAngle = -140;
        label.maximumValueAngle = 160;
        compare(label.angleRange, 300);

        label.minimumValueAngle = -40;
        label.maximumValueAngle = -10;
        compare(label.angleRange, 30);

        label.minimumValueAngle = -40;
        label.maximumValueAngle = 10;
        compare(label.angleRange, 50);
    }

    function test_tickmarksAndLabels() {
        label.minimumValueAngle = 0;
        label.maximumValueAngle = 180;

        // When the label step size is 40 and the maximumValue 220, the following labels should be displayed:
        // 0, 40, 80, 120, 160, 200
        // In addition, the labels should be positioned according to how much of the angle range they actually use;
        // since 240 is unable to be displayed, 200 should not be displayed on the last tickmark, but two tickmarks
        // before it (the tickmarkStepSize is 10).
        label.maximumValue = 220;
        label.tickmarkStepSize = 10;
        label.labelStepSize = 40;
        compare(label.angleRange, 180);
        compare(label.valueToAngle(0), 0);
        compare(label.valueToAngle(220), 180);
        compare(label.__panel.labelAngleFromIndex(1), 32.7272727);
        compare(label.__panel.labelAngleFromIndex(0), 0);
        compare(label.__panel.totalMinorTickmarkCount, 88);
        compare(label.__panel.tickmarkValueFromIndex(0), 0);
        compare(label.__panel.tickmarkValueFromIndex(label.tickmarkCount - 1), 220);
        compare(label.__panel.tickmarkValueFromMinorIndex(0), 2);
        compare(label.__panel.tickmarkValueFromMinorIndex(label.minorTickmarkCount - 1), 8);
        compare(label.__panel.tickmarkValueFromMinorIndex(((label.tickmarkCount - 1) * label.minorTickmarkCount) - 1), 218);

        var rotations = [
            -0.5729577951308232,
            7.6088603866873585,
            15.79067856850554,
            23.972496750323725,
            32.1543149321419,
            40.33613311396008,
            48.51795129577827,
            56.69976947759645,
            64.88158765941463,
            73.06340584123282,
            81.245224023051,
            89.42704220486918,
            97.60886038668737,
            105.79067856850554,
            113.97249675032373,
            122.1543149321419,
            130.33613311396007,
            138.51795129577826,
            146.69976947759645,
            154.8815876594146,
            163.0634058412328,
            171.24522402305098,
            179.42704220486917
        ];

        // Check that the tickmarks have the correct transforms and hence are actually in the correct position.
        for (var tickmarkIndex = 0; tickmarkIndex < label.__tickmarkCount; ++tickmarkIndex) {
            var tickmark = TestUtils.findChild(label.__panel, "tickmark" + tickmarkIndex);
            verify(tickmark);
            compare(tickmark.transform[1].angle, rotations[tickmarkIndex]);
        }

        var minorRotations = [
            1.3498847387982247, 2.986248375161861,
            4.622612011525496, 6.258975647889133, 9.531702920616407,
            11.168066556980042, 12.804430193343679, 14.440793829707316,
            17.713521102434587, 19.349884738798224, 20.98624837516186,
            22.622612011525494, 25.89533928425277, 27.531702920616407,
            29.16806655698004, 30.80443019334368, 34.07715746607095,
            35.71352110243459, 37.349884738798224, 38.986248375161864,
            42.25897564788913, 43.89533928425277, 45.531702920616404,
            47.168066556980044, 50.44079382970732, 52.07715746607096,
            53.71352110243459, 55.34988473879823, 58.6226120115255,
            60.25897564788914, 61.89533928425277, 63.53170292061641,
            66.80443019334368, 68.4407938297073, 70.07715746607094,
            71.71352110243458, 74.98624837516186, 76.62261201152549,
            78.25897564788913, 79.89533928425277, 83.16806655698004,
            84.80443019334366, 86.4407938297073, 88.07715746607094,
            91.34988473879822, 92.98624837516185, 94.62261201152549,
            96.25897564788913, 99.53170292061641, 101.16806655698004,
            102.80443019334368, 104.44079382970732, 107.71352110243458,
            109.34988473879821, 110.98624837516185, 112.62261201152549,
            115.89533928425277, 117.5317029206164, 119.16806655698004,
            120.80443019334368, 124.07715746607094, 125.71352110243457,
            127.34988473879821, 128.98624837516184, 132.25897564788912,
            133.89533928425277, 135.5317029206164, 137.16806655698002,
            140.4407938297073, 142.07715746607096, 143.71352110243458,
            145.3498847387982, 148.6226120115255, 150.25897564788914,
            151.89533928425277, 153.5317029206164, 156.80443019334365,
            158.4407938297073, 160.07715746607093, 161.71352110243456,
            164.98624837516184, 166.6226120115255, 168.25897564788912,
            169.89533928425274, 173.16806655698002, 174.80443019334368,
            176.4407938297073, 178.07715746607093,
        ];

        for (var minorTickmarkIndex = 0; minorTickmarkIndex < label.__panel.totalMinorTickmarkCount; ++minorTickmarkIndex) {
            var minorTickmark = TestUtils.findChild(label.__panel, "minorTickmark" + minorTickmarkIndex);
            verify(minorTickmark);
            compare(minorTickmark.transform[1].angle, minorRotations[minorTickmarkIndex]);
        }

        // 0, 10, 20... 100
        label.labelStepSize = 10;
        label.minimumValue = 0;
        label.maximumValue = 100;
        compare(label.tickmarkCount, 11);
        compare(label.minorTickmarkCount, 4);
        compare(label.labelCount, 11);
        compare(label.angleRange, 180);
        compare(label.valueToAngle(0), 0);
        compare(label.valueToAngle(100), 180);
        compare(label.__panel.labelAngleFromIndex(0), 0);
        compare(label.__panel.labelAngleFromIndex(label.labelCount - 1), 180);
        compare(label.__panel.totalMinorTickmarkCount, 40);
        compare(label.__panel.tickmarkValueFromIndex(0), 0);
        compare(label.__panel.tickmarkValueFromIndex(label.tickmarkCount - 1), 100);
        compare(label.__panel.tickmarkValueFromMinorIndex(0), 2);
        compare(label.__panel.tickmarkValueFromMinorIndex(label.minorTickmarkCount - 1), 8);
        compare(label.__panel.tickmarkValueFromMinorIndex(((label.tickmarkCount - 1) * label.minorTickmarkCount) - 1), 98);

        // 10, 20... 100
        label.labelStepSize = 10;
        label.minimumValue = 10;
        label.maximumValue = 100;

        compare(label.tickmarkCount, 10);
        compare(label.minorTickmarkCount, 4);
        compare(label.labelCount, 10);
        compare(label.angleRange, 180);
        compare(label.valueToAngle(10), 0);
        compare(label.valueToAngle(100), 180);
        compare(label.__panel.labelAngleFromIndex(0), 0);
        compare(label.__panel.labelAngleFromIndex(label.labelCount - 1), 180);
        compare(label.__panel.totalMinorTickmarkCount, 36);
        compare(label.__panel.tickmarkValueFromIndex(0), 10);
        compare(label.__panel.tickmarkValueFromIndex(label.tickmarkCount - 1), 100);
        compare(label.__panel.tickmarkValueFromMinorIndex(0), 12);
        compare(label.__panel.tickmarkValueFromMinorIndex(label.minorTickmarkCount - 1), 18);
        compare(label.__panel.tickmarkValueFromMinorIndex(((label.tickmarkCount - 1) * label.minorTickmarkCount) - 1), 98);

        // -10, 0, 10, 20... 100
        label.labelStepSize = 10;
        label.minimumValue = -10;
        label.maximumValue = 100;
        compare(label.tickmarkCount, 12);
        compare(label.minorTickmarkCount, 4);
        compare(label.labelCount, 12);
        compare(label.angleRange, 180);
        compare(label.valueToAngle(-10), 0);
        compare(label.valueToAngle(100), 180);
        compare(label.__panel.labelAngleFromIndex(0), 0);
        compare(label.__panel.labelAngleFromIndex(label.labelCount - 1), 180);
        compare(label.__panel.totalMinorTickmarkCount, 44);
        compare(label.__panel.tickmarkValueFromIndex(0), -10);
        compare(label.__panel.tickmarkValueFromIndex(label.tickmarkCount - 1), 100);
        compare(label.__panel.tickmarkValueFromMinorIndex(0), -8);
        compare(label.__panel.tickmarkValueFromMinorIndex(label.minorTickmarkCount - 1), -2);
        compare(label.__panel.tickmarkValueFromMinorIndex(((label.tickmarkCount - 1) * label.minorTickmarkCount) - 1), 98);

        // 0, 10, 20... 105
        label.labelStepSize = 10;
        label.minimumValue = 0;
        label.maximumValue = 105;
        label.minorTickmarkCount = 1;
        compare(label.tickmarkCount, 11);
        compare(label.labelCount, 11);
        compare(label.angleRange, 180);
        compare(label.valueToAngle(0), 0);
        compare(label.valueToAngle(105), 180);
        compare(label.__panel.labelAngleFromIndex(0), 0);
        compare(label.__panel.labelAngleFromIndex(label.labelCount - 1), 171.42857142857142);
        compare(label.__panel.tickmarkAngleFromIndex(label.tickmarkCount - 1), 171.42857142857142);
        compare(label.__panel.minorTickmarkAngleFromIndex(label.minorTickmarkCount * (label.tickmarkCount - 1)), 180);
        compare(label.__panel.totalMinorTickmarkCount, 11);
        compare(label.__panel.tickmarkValueFromIndex(0), 0);
        compare(label.__panel.tickmarkValueFromIndex(label.tickmarkCount - 1), 105);
        compare(label.__panel.tickmarkValueFromMinorIndex(0), 5.25);
        compare(label.__panel.tickmarkValueFromMinorIndex(((label.tickmarkCount - 1) * label.minorTickmarkCount) - 1), 99.75);

        // 0, 10, 20... 101. Shouldn't show an extra minor tickmark, because each minor tickmark
        // is 2 "units", and 1 (taken from 101) < 2.
        label.labelStepSize = 10;
        label.minimumValue = 0;
        label.maximumValue = 101;
        label.minorTickmarkCount = 4;
        compare(label.valueToAngle(0), 0);
        compare(label.valueToAngle(101), 180);
        compare(label.tickmarkCount, 11);
        compare(label.labelCount, 11);
        compare(label.angleRange, 180);
        compare(label.__panel.labelAngleFromIndex(0), 0);
        compare(label.__panel.labelAngleFromIndex(label.labelCount - 1), 178.21782178217825);
        compare(label.__panel.tickmarkAngleFromIndex(label.tickmarkCount - 1), 178.21782178217825);
        compare(label.__panel.totalMinorTickmarkCount, 40);
        compare(label.__panel.tickmarkValueFromIndex(0), 0);
        compare(label.__panel.tickmarkValueFromIndex(label.tickmarkCount - 1), 101);
        compare(label.__panel.tickmarkValueFromMinorIndex(0), 2.02);
        compare(label.__panel.tickmarkValueFromMinorIndex(label.minorTickmarkCount - 1), 8.08);
        compare(label.__panel.tickmarkValueFromMinorIndex(((label.tickmarkCount - 1) * label.minorTickmarkCount) - 1), 98.97999999999999);

        // Test reversed labels.
        label.minimumValueAngle = 270;
        label.maximumValueAngle = 90;
        label.minimumValue = 0;
        label.maximumValue = 100;
        compare(label.valueToAngle(0), 270);
        compare(label.valueToAngle(50), 180);
        compare(label.valueToAngle(100), 90);

        label.minimumValueAngle = 270;
        label.maximumValueAngle = 0;
        compare(label.valueToAngle(0), 270);
        compare(label.valueToAngle(100), 0);

        label.minimumValueAngle = -90;
        label.maximumValueAngle = 0;
        compare(label.valueToAngle(0), -90);
        compare(label.valueToAngle(100), 0);

        label.minimumValueAngle = -180;
        label.maximumValueAngle = 90;
        compare(label.valueToAngle(0), -180);
        compare(label.valueToAngle(100), 90);

        label.minimumValueAngle = 215;
        label.maximumValueAngle = 145;
        label.minimumValue = 0;
        label.maximumValue = 100;
        compare(label.tickmarkCount, 11);
        compare(label.labelCount, 11);
        compare(label.angleRange, -70);
        compare(label.__panel.totalMinorTickmarkCount, 40);
        compare(label.__panel.tickmarkValueFromIndex(0), 0);
        compare(label.__panel.tickmarkValueFromIndex(label.tickmarkCount - 1), 100);
        compare(label.__panel.tickmarkValueFromMinorIndex(0), 2);
        compare(label.__panel.tickmarkValueFromMinorIndex(label.minorTickmarkCount - 1), 8);
        compare(label.__panel.tickmarkValueFromMinorIndex(((label.tickmarkCount - 1) * label.minorTickmarkCount) - 1), 98);
    }

    function test_labelText() {
        for (var i = 0; i < label.labelCount; ++i) {
            var labelDelegateLoader = findChild(label, "labelDelegateLoader" + i);
            verify(labelDelegateLoader);
            compare(labelDelegateLoader.styleData.index, i);
            compare(labelDelegateLoader.styleData.value, i * label.labelStepSize);
        }
    }

    function test_invalidValues() {
        // Shouldn't produce warnings.
        label.labelStepSize = 0;
        label.tickmarkStepSize = 0;
    }
}
