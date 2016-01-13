/*****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Add-On Graphical Effects module.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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
*****************************************************************************/

import QtQuick 2.0

ListModel {
    id: testcaseModel
    ListElement { name: "TestBlend.qml"; group: "Blend"; last: true }
    ListElement { name: "TestDisplace.qml"; group: "Distortion"; last: true }
    ListElement { name: "TestOpacityMask.qml"; group: "Mask" }
    ListElement { name: "TestThresholdMask.qml"; group: "Mask"; last: true }
    ListElement { name: "TestGlow.qml"; group: "Glow" }
    ListElement { name: "TestRectangularGlow.qml"; group: "Glow"; last: true }
    ListElement { name: "TestFastBlur.qml"; group: "Blur" }
    ListElement { name: "TestGaussianBlur.qml"; group: "Blur" }
    ListElement { name: "TestMaskedBlur.qml"; group: "Blur" }
    ListElement { name: "TestRecursiveBlur.qml"; group: "Blur"; last: true }
    ListElement { name: "TestDirectionalBlur.qml"; group: "Motion Blur";  }
    ListElement { name: "TestRadialBlur.qml"; group: "Motion Blur";  }
    ListElement { name: "TestZoomBlur.qml"; group: "Motion Blur"; last: true }
    ListElement { name: "TestDropShadow.qml"; group: "Drop Shadow" }
    ListElement { name: "TestInnerShadow.qml"; group: "Drop Shadow"; last: true }
    ListElement { name: "TestLinearGradient.qml"; group: "Gradient" }
    ListElement { name: "TestConicalGradient.qml"; group: "Gradient" }
    ListElement { name: "TestRadialGradient.qml"; group: "Gradient"; last: true }
    ListElement { name: "TestColorize.qml"; group: "Color" }
    ListElement { name: "TestColorOverlay.qml"; group: "Color" }
    ListElement { name: "TestHueSaturation.qml"; group: "Color" }
    ListElement { name: "TestBrightnessContrast.qml"; group: "Color" }
    ListElement { name: "TestDesaturate.qml"; group: "Color" }
    ListElement { name: "TestLevelAdjust.qml"; group: "Color" }
    ListElement { name: "TestGammaAdjust.qml"; group: "Color"; last: true }
}
