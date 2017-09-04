/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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
import QtQuick.Particles 2.0

Rectangle {
    id: root;
    width: 360
    height: 600
    color: "black"

    function newPithySaying() {
        switch (Math.floor(Math.random()*16)) {
            case 0: return "Hello World";
            case 1: return "G'day Mate";
            case 2: return "Code Less";
            case 3: return "Create More";
            case 4: return "Deploy Everywhere";
            case 5: return "Qt Meta-object Language";
            case 6: return "Qt Magic Language";
            case 7: return "Fluid UIs";
            case 8: return "Touchable";
            case 9: return "How's it going?";
            case 10: return "Do you like text?";
            case 11: return "Enjoy!";
            case 12: return "ERROR: Out of pith";
            case 13: return "Punctuation Failure";
            case 14: return "I can go faster";
            case 15: return "I can go slower";
            default: return "OMGWTFBBQ";
        }
    }

    ParticleSystem {
        anchors.fill: parent
        id: syssy
        MouseArea {
            anchors.fill: parent
            onClicked: syssy.running = !syssy.running
        }
        Emitter {
            anchors.centerIn: parent
            emitRate: 1
            lifeSpan: 4800
            lifeSpanVariation: 1600
            velocity: AngleDirection {angleVariation: 360; magnitude: 40; magnitudeVariation: 20}
        }
        ItemParticle {
            delegate: Text {
                text: root.newPithySaying();
                color: "white"
                font.pixelSize: 18
                font.bold: true
            }
        }
    }
}
