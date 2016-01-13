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