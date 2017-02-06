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

Item {
    id: imageparticleelementtest
    anchors.fill: parent
    property string testtext: ""

    ParticleSystem {
        id: particlesystemelement
        anchors.fill: parent
        ImageParticle {
            id: imageparticle
            source: "pics/smile.png"
            color: "red"
            Behavior on color { ColorAnimation { duration: 3000 } }
        }
        Emitter {
            id: particleemitter
            anchors.centerIn: parent
            emitRate: 50
            lifeSpan: 3000
            velocity: AngleDirection { angle: 0; angleVariation: 360; magnitude: 60 }
        }
    }


    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: imageparticleelementtest
                testtext: "This is an ImageParticle element. It should be emitting particles "+
                "from the center of the display.\n"+
                "Next, let's change the color of the particles." }
        },
        State { name: "green"; when: statenum == 2
            PropertyChanges { target: imageparticle; color: "lightgreen" }
            PropertyChanges { target: imageparticleelementtest
                testtext: "The particles should now be green.\n"+
                "Next, let's get them spinning." }
        },
        State { name: "spinning"; when: statenum == 3
            PropertyChanges { target: imageparticle; color: "lightgreen"; rotation: 360; rotationVelocity: 100 }
            PropertyChanges { target: imageparticleelementtest
                testtext: "The particles should now be green and spinning.\n"+
                "Next, let's get them popping in and out." }
        },
        State { name: "scaling"; when: statenum == 4
            PropertyChanges { target: imageparticle; color: "lightgreen"; rotation: 360; rotationVelocity: 100; entryEffect: ImageParticle.Scale }
            PropertyChanges { target: imageparticleelementtest
                testtext: "The particles should now be scaling in and out.\n"+
                "Advance to restart the test." }
        }
    ]
}