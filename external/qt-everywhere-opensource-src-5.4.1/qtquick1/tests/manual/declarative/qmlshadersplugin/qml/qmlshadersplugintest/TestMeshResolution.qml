/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
import QtQuick 1.0
import Qt.labs.shaders 1.0

Rectangle {
    anchors.fill: parent;
    color: "red"

    Timer {
        running: true
        interval: 2000
        repeat: true
        onTriggered: {
            effect.meshResolution.width == 1 ? effect.meshResolution = Qt.size(40,40) : effect.meshResolution = Qt.size(1,1)
        }
    }

    Rectangle {
        id: thesource
        anchors.centerIn: parent;
        color: "green"
        width: parent.width
        height: parent.height
        Image {
            anchors.centerIn: parent;
            source: "image_opaque.png"
        }
    }


    ShaderEffectItem {
        id: effect
        anchors.fill: thesource;
        property variant source: ShaderEffectSource { sourceItem: thesource }

        vertexShader: "
           attribute highp vec4 qt_Vertex;
           attribute highp vec2 qt_MultiTexCoord0;
           uniform highp mat4 qt_ModelViewProjectionMatrix;
           varying highp vec2 qt_TexCoord0;
           void main() {
               qt_TexCoord0 = qt_MultiTexCoord0;

               highp vec4 shift = vec4(cos(1071. * qt_MultiTexCoord0.x + 1.0) + sin(2051. * qt_MultiTexCoord0.y + 1.0),
                                       cos(1131. * qt_MultiTexCoord0.x + 1.0) + sin(3039. * qt_MultiTexCoord0.x + 1.0), 0, 0) * 3.0;

               if (qt_MultiTexCoord0.x < 0.01 || qt_MultiTexCoord0.x > 0.99)
                   shift.x = 0.;
               if (qt_MultiTexCoord0.y < 0.01 || qt_MultiTexCoord0.y > 0.99)
                   shift.y = 0.;

               gl_Position = qt_ModelViewProjectionMatrix * (qt_Vertex + shift);
           }
        "
    }

    Rectangle {
        width: parent.width
        height: 40
        color: "#cc000000"
        Text {
             id: label
             anchors.centerIn:  parent
             text: effect.meshResolution.width == 1 ? "Resolution (1,1) (image looks normal)" : "Resolution (40,40) (image looks distorted)"
             color: "white"
             font.bold: true
        }
    }
}
