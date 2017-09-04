/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
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

import QtQuick 2.2

Rectangle {

    width: 320
    height: 480

    color: "steelblue"

    Grid {
        anchors.fill: parent
        anchors.margins: 20
        columnSpacing: 30
        rowSpacing: 30
        columns: 4
        rows: 4

        property real cellWidth: (width - (columns - 1) * columnSpacing) / columns;
        property real cellHeight: (height - (rows - 1) * rowSpacing) / rows;

        Item {
            width: parent.cellWidth
            height: parent.cellHeight
//! [x on]
Rectangle {
    width: 50
    height: 50
    color: "lightsteelblue"
    XAnimator on x {
        from: 10;
        to: 0;
        duration: 1000
    }
}
//! [x on]
        }
        Item {
            width: parent.cellWidth
            height: parent.cellHeight
//! [x target]
Rectangle {
    id: xmovingBox
    width: 50
    height: 50
    color: "lightsteelblue"
    XAnimator {
        target: xmovingBox;
        from: 10;
        to: 0;
        duration: 1000
        running: true
    }
}
//! [x target]
    }
    Item {
        width: parent.cellWidth
        height: parent.cellHeight
//! [y on]
Rectangle {
    width: 50
    height: 50
    color: "lightsteelblue"
    YAnimator on y {
        from: 10;
        to: 0;
        duration: 1000
    }
}
//! [y on]
    }
    Item {
        width: parent.cellWidth
        height: parent.cellHeight
//! [y target]
Rectangle {
    id: ymovingBox
    width: 50
    height: 50
    color: "lightsteelblue"
    YAnimator {
        target: ymovingBox;
        from: 10;
        to: 0;
        duration: 1000
        running: true
    }
}
//! [y target]
    }
//! [rotation on]
Rectangle {
    width: 50
    height: 50
    color: "lightsteelblue"
    RotationAnimator on rotation {
        from: 0;
        to: 360;
        duration: 1000
    }
}
//! [rotation on]
//! [rotation target]
Rectangle {
    id: rotatingBox
    width: 50
    height: 50
    color: "lightsteelblue"
    RotationAnimator {
        target: rotatingBox;
        from: 0;
        to: 360;
        duration: 1000
        running: true
    }
}
//! [rotation target]
//! [scale on]
Rectangle {
    width: 50
    height: 50
    color: "lightsteelblue"
    ScaleAnimator on scale {
        from: 0.5;
        to: 1;
        duration: 1000
    }
}
//! [scale on]
//! [scale target]
Rectangle {
    id: scalingBox
    width: 50
    height: 50
    color: "lightsteelblue"
    ScaleAnimator {
        target: scalingBox;
        from: 0.5;
        to: 1;
        duration: 1000
        running: true
    }
}
//! [scale target]
//! [opacity on]
Rectangle {
    width: 50
    height: 50
    color: "lightsteelblue"
    OpacityAnimator on opacity{
        from: 0;
        to: 1;
        duration: 1000
    }
}
//! [opacity on]
//! [opacity target]
Rectangle {
    id: opacityBox
    width: 50
    height: 50
    color: "lightsteelblue"
    OpacityAnimator {
        target: opacityBox;
        from: 0;
        to: 1;
        duration: 1000
        running: true
    }
}
//! [opacity target]
//![shaderon]
ShaderEffect {
    width: 50
    height: 50
    property variant t;
    UniformAnimator on t {
        from: 0
        to: 1
        duration: 1000
    }
    fragmentShader:
    "
        uniform lowp float t;
        varying highp vec2 qt_TexCoord0;
        void main() {
            lowp float c = qt_TexCoord0.y;
            gl_FragColor = vec4(c * t, 0, 0, 1);
        }
    "
}
//![shaderon]
//![shader target]
ShaderEffect {
    id: shader
    width: 50
    height: 50
    property variant t;
    UniformAnimator {
        target: shader
        uniform: "t"
        from: 0
        to: 1
        duration: 1000
        running: true
    }
    fragmentShader:
    "
        uniform lowp float t;
        varying highp vec2 qt_TexCoord0;
        void main() {
            lowp float c = qt_TexCoord0.y;
            gl_FragColor = vec4(0, 0, c * t, 1);
        }
    "
}
//![shader target]
//![mixed]
Rectangle {
    id: mixBox
    width: 50
    height: 50
    ParallelAnimation {
        ColorAnimation {
            target: mixBox
            property: "color"
            from: "forestgreen"
            to: "lightsteelblue";
            duration: 1000
        }
        ScaleAnimator {
            target: mixBox
            from: 2
            to: 1
            duration: 1000
        }
        running: true
    }
}
//! [mixed]
    }
}
