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
import QtQuick.Particles 2.0

Rectangle {
    color: "black"
    width: 320
    height: 320

    ShaderEffect {
        id: sei
        property variant source
    }

    ShaderEffectSource {
        id: doomedses
        hideSource: true
        sourceItem: Image {
            id: doomed
            source: "star.png"
        }
    }

    function setDeletedSourceItem() {
        doomed.destroy();
        sei.source = doomedses;
        // now set a fragment shader to trigger source texture detection.
        sei.fragmentShader = "varying highp vec2 qt_TexCoord0;\
                              uniform sampler2D source;\
                              uniform lowp float qt_Opacity;\
                              void main() {\
                                  gl_FragColor = texture2D(source, qt_TexCoord0) * qt_Opacity;\
                              }";
    }
}
