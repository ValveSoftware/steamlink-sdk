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

    ParticleSystem {
        id: sys
        objectName: "system"
        anchors.fill: parent

        ImageParticle {
            source: "../../shared/star.png"
            rotation: 90
        }

        Emitter{
            //0,0 position
            size: 32
            emitRate: 1000
            lifeSpan: 500
        }

        Affector {
            once: true
            onAffectParticles: {
                for (var i=0; i<particles.length; i++) {
                    particles[i].initialX = 100;
                    particles[i].initialY = 100;
                    particles[i].initialVX = 100;
                    particles[i].initialVY = 100;
                    particles[i].initialAX = 100;
                    particles[i].initialAY = 100;
                    particles[i].startSize = 100;
                    particles[i].endSize = 100;
                    particles[i].autoRotate = true;
                    particles[i].update = true;
                    particles[i].red = 0;
                    particles[i].green = 1.0;
                    particles[i].blue = 0;
                    particles[i].alpha = 0;
                }
            }
        }
    }
}
