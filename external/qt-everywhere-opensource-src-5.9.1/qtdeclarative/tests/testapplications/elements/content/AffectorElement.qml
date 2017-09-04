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
    id: affectorelementtest
    anchors.fill: parent
    property string testtext: ""

    ParticleSystem {
        id: particlesystem
        anchors.fill: parent

        ImageParticle {
            id: imageparticle
            source: "pics/star.png"
            color: "blue"
            entryEffect: ImageParticle.None
            anchors.fill: parent
        }

        // Pipe
        Rectangle {
            id: pipe
            x: 0; y: 300
            border.color: "black"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "lightgray" }
                GradientStop { position: 1.0; color: "gray" }
            }
            height: 40; width: 40
        }
        Rectangle {
            id: pipehead
            anchors.left: pipe.right
            anchors.verticalCenter: pipe.verticalCenter
            border.color: "black"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "lightgray" }
                GradientStop { position: 1.0; color: "gray" }
            }
            height: 50; width: 10
        }

        Emitter {
            id: emitterelement
            anchors.left: pipe.left
            anchors.leftMargin: 10
            anchors.bottom: pipe.bottom
            anchors.bottomMargin: 8
            height: 5
            emitRate: 100
            lifeSpan: 10000
            velocity: AngleDirection { angle: 0; magnitude: 30 }
        }

        // Affectors
        Gravity {
            id: gravity
            x: pipe.width; y: pipe.y-100
            enabled: false
            height: 200
            width: parent.width - pipe.width
            angle: 90
            acceleration: 30
        }
        Wander {
            id: wander
            enabled: false
            anchors.verticalCenter: pipe.verticalCenter
            anchors.left: pipe.right
            height: pipe.height
            width: 5
            xVariance: 50
            yVariance: 100
            pace: 200
        }
        Turbulence {
            id: turbulence
            enabled: false
            strength: 40
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 100
            width: parent.width; height: 100
        }
        Friction {
            id: friction
            anchors.bottom: parent.bottom; width: parent.width; height: 100
            enabled: false
            factor: 2
        }
        Age {
            id: age
            anchors.bottom: parent.bottom; width: 360; height: 5
        }

    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: affectorelementtest
                testtext: "This is a group of currently disabled Affector elements. "+
                "A blue stream of particles should be flowing from a block to the left.\n"+
                "Next, let's add some variance in direction when the particles leave the block." }
        },
        State { name: "spread"; when: statenum == 2
            PropertyChanges { target: wander; enabled: true }
            PropertyChanges { target: affectorelementtest
                testtext: "The particles should be spreading out as they progress.\n"+
                "Next, let's introduce gravity." }
        },
        State { name: "gravity"; when: statenum == 3
            PropertyChanges { target: wander; enabled: true }
            PropertyChanges { target: gravity; enabled: true }
            PropertyChanges { target: affectorelementtest
            testtext: "The particles should now be dropping.\n"+
                "Also, no particles should be visible below the bounds of the application, "+
                "i.e. the white panel.\n"+
                "Next, let's introduce some friction at the bottom of the display." }
        },
        State { name: "friction"; when: statenum == 4
            PropertyChanges { target: wander; enabled: true }
            PropertyChanges { target: gravity; enabled: true }
            PropertyChanges { target: friction; enabled: true }
            PropertyChanges { target: affectorelementtest
                testtext: "The particles should now be decelerating suddenly at the bottom.\n"+
                "Next, let's add some turbulence to the flow." }
        },
        State { name: "turbulence"; when: statenum == 5
            PropertyChanges { target: wander; enabled: true }
            PropertyChanges { target: gravity; enabled: true }
            PropertyChanges { target: friction; enabled: true }
            PropertyChanges { target: turbulence; enabled: true }
            PropertyChanges { target: affectorelementtest
                testtext: "The particles should now be turbulent.\n"+
                "Advance to restart the test." }
        }
    ]
}