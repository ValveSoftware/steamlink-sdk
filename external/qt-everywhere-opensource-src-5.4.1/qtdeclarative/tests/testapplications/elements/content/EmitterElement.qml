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
    id: emitterelementtest
    anchors.fill: parent
    property string testtext: ""

    ParticleSystem {
        id: particlesystem
        anchors.fill: parent
        ImageParticle {
            id: imageparticle
            source: "pics/smile.png"
            color: "red"
            Behavior on color { ColorAnimation { duration: 3000 } }
        }
        Emitter {
            id: emitterelement
            anchors.centerIn: parent
            property int emitvelocity: 10
            emitRate: 5
            lifeSpan: 1000
            velocity: AngleDirection { angle: 0; angleVariation: 360; magnitude: emitterelement.emitvelocity }
        }
    }


    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: emitterelementtest
                testtext: "This is an Emitter element, visualized by an ImageParticle. It should be emitting particles "+
                "slowly from the center of the display.\n"+
                "Next, let's change the emission velocity of the particles." }
        },
        State { name: "fast"; when: statenum == 2
            PropertyChanges { target: emitterelement; emitvelocity: 50 }
            PropertyChanges { target: emitterelementtest
                testtext: "The particles emitted should be moving more quickly.\n"+
                "Next, let's increase the number of particles emitted." }
        },
        State { name: "many"; when: statenum == 3
            PropertyChanges { target: emitterelement; emitvelocity: 50; emitRate: 100 }
            PropertyChanges { target: emitterelementtest
            testtext: "The particles should now be quick and numerous.\n"+
                "Next, let's allow them to survive longer." }
        },
        State { name: "enduring"; when: statenum == 4
            PropertyChanges { target: emitterelement; emitvelocity: 50; emitRate: 100; lifeSpan: 3000 }
            PropertyChanges { target: emitterelementtest
                testtext: "The particles should now be enduring to the edges of the display.\n"+
                "Next, let's have them changing their size." }
        },
        State { name: "sized"; when: statenum == 5
            PropertyChanges { target: emitterelement; emitvelocity: 50; emitRate: 100; lifeSpan: 3000; size: 20; endSize: 5 }
            PropertyChanges { target: emitterelementtest
                testtext: "The particles should now be starting large and ending small.\n"+
                "Advance to restart the test." }
        }
    ]
}