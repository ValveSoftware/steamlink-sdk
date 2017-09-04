/*****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Add-On Graphical Effects module.
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
