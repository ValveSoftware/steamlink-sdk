/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.4

Canvas {
    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();

        ctx.scale(width, height);

        ctx.beginPath();
        ctx.moveTo(0.706,0.542);
        ctx.bezierCurveTo(0.709,0.527,0.711,0.512,0.711,0.49700000000000005);
        ctx.bezierCurveTo(0.711,0.4820000000000001,0.709,0.465,0.706,0.451);
        ctx.lineTo(0.752,0.41600000000000004);
        ctx.lineTo(0.759,0.41);
        ctx.lineTo(0.756,0.4);
        ctx.bezierCurveTo(0.756,0.399,0.749,0.382,0.737,0.36200000000000004);
        ctx.bezierCurveTo(0.725,0.3420000000000001,0.714,0.328,0.714,0.327);
        ctx.lineTo(0.708,0.319);
        ctx.lineTo(0.698,0.324);
        ctx.lineTo(0.645,0.346);
        ctx.bezierCurveTo(0.623,0.32499999999999996,0.595,0.309,0.5650000000000001,0.3);
        ctx.lineTo(0.558,0.243);
        ctx.lineTo(0.557,0.23299999999999998);
        ctx.lineTo(0.547,0.23099999999999998);
        ctx.bezierCurveTo(0.546,0.23099999999999998,0.528,0.22799999999999998,0.505,0.22799999999999998);
        ctx.bezierCurveTo(0.481,0.22799999999999998,0.463,0.23099999999999998,0.463,0.23099999999999998);
        ctx.lineTo(0.453,0.23299999999999998);
        ctx.lineTo(0.452,0.243);
        ctx.lineTo(0.444,0.299);
        ctx.bezierCurveTo(0.41400000000000003,0.308,0.387,0.324,0.364,0.345);
        ctx.lineTo(0.312,0.323);
        ctx.lineTo(0.302,0.319);
        ctx.lineTo(0.296,0.327);
        ctx.bezierCurveTo(0.296,0.327,0.284,0.342,0.27299999999999996,0.362);
        ctx.bezierCurveTo(0.26,0.383,0.254,0.399,0.254,0.4);
        ctx.lineTo(0.25,0.41);
        ctx.lineTo(0.258,0.416);
        ctx.lineTo(0.303,0.45099999999999996);
        ctx.bezierCurveTo(0.3,0.465,0.299,0.48,0.299,0.497);
        ctx.bezierCurveTo(0.299,0.513,0.3,0.528,0.304,0.543);
        ctx.lineTo(0.259,0.577);
        ctx.lineTo(0.25,0.584);
        ctx.lineTo(0.254,0.593);
        ctx.bezierCurveTo(0.254,0.594,0.261,0.61,0.273,0.63);
        ctx.bezierCurveTo(0.28500000000000003,0.65,0.29500000000000004,0.664,0.29600000000000004,0.665);
        ctx.lineTo(0.30200000000000005,0.673);
        ctx.lineTo(0.31200000000000006,0.669);
        ctx.lineTo(0.36500000000000005,0.647);
        ctx.bezierCurveTo(0.38700000000000007,0.668,0.41400000000000003,0.684,0.44400000000000006,0.6930000000000001);
        ctx.lineTo(0.45200000000000007,0.7510000000000001);
        ctx.lineTo(0.45300000000000007,0.7620000000000001);
        ctx.lineTo(0.4640000000000001,0.7630000000000001);
        ctx.bezierCurveTo(0.4640000000000001,0.7630000000000001,0.4820000000000001,0.7640000000000001,0.5060000000000001,0.7640000000000001);
        ctx.bezierCurveTo(0.5300000000000001,0.7640000000000001,0.5470000000000002,0.7620000000000001,0.5480000000000002,0.7620000000000001);
        ctx.lineTo(0.5580000000000002,0.7610000000000001);
        ctx.lineTo(0.5590000000000002,0.7510000000000001);
        ctx.lineTo(0.5660000000000002,0.6940000000000001);
        ctx.bezierCurveTo(0.5960000000000002,0.685,0.6230000000000002,0.669,0.6460000000000001,0.648);
        ctx.lineTo(0.6990000000000002,0.67);
        ctx.lineTo(0.7090000000000002,0.674);
        ctx.lineTo(0.7150000000000002,0.665);
        ctx.bezierCurveTo(0.7150000000000002,0.665,0.7260000000000002,0.65,0.7370000000000002,0.63);
        ctx.bezierCurveTo(0.7490000000000002,0.61,0.7560000000000002,0.594,0.7560000000000002,0.593);
        ctx.lineTo(0.7600000000000002,0.584);
        ctx.lineTo(0.751,0.577);
        ctx.lineTo(0.706,0.542);
        ctx.closePath();
        ctx.moveTo(0.505,0.622);
        ctx.bezierCurveTo(0.436,0.622,0.38,0.566,0.38,0.497);
        ctx.bezierCurveTo(0.38,0.428,0.436,0.372,0.505,0.372);
        ctx.bezierCurveTo(0.5740000000000001,0.372,0.63,0.428,0.63,0.497);
        ctx.bezierCurveTo(0.63,0.565,0.574,0.622,0.505,0.622);
        ctx.closePath();
        ctx.fillStyle = "#333333";
        ctx.fill();
    }
}
