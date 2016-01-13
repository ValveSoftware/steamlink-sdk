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
    id: directionelementtest
    anchors.fill: parent
    property string testtext: ""

    ParticleSystem {
        id: particlesystem
        anchors.fill: parent
        Item {
            id: targetbox
            x: 50
            y: 300
            Rectangle {
                id: targeticon
                color: "brown"
                height: 0; width: 0; radius: 20
                anchors.centerIn: parent
                Behavior on height { NumberAnimation { duration: 500; easing.type: Easing.OutBounce } }
                Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutBounce } }
            }
        }
        ImageParticle {
            id: imgparticle
            source: "pics/star.png"
            color: "red"
            entryEffect: ImageParticle.None
        }
        Emitter {
            id: emitter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 5
            anchors.horizontalCenter: parent.horizontalCenter
            emitRate: 50
            lifeSpan: 4000
            size: 20
            velocity: angledirectionelement
            AngleDirection { id: angledirectionelement; angle: -75; angleVariation: 5; magnitude: 150 }
            TargetDirection { id: targetdirectionelement; targetItem: targetbox; targetVariation: 10; magnitude: 150 }
            PointDirection { id: pointdirectionelement; y: -100; xVariation: 10; yVariation: 10; }
            PointDirection { id: pointdirectionelementdownward; y: 200 }
            CumulativeDirection {
                id: cumulativedirectionelement
                PointDirection { y: -200 }
                AngleDirection { angle: 270; angleVariation: 45; magnitude: 100; magnitudeVariation: 10; }
            }
        }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: directionelementtest
                testtext: "This is an Emitter with the direction set by an AngleDirection.\n"+
                "The particles should be firing at a -75 degree (to the top right) .\n"+
                "Next, let's change the Emitter to target an item." }
        },
        State { name: "ontarget"; when: statenum == 2
            PropertyChanges { target: emitter; velocity: targetdirectionelement }
            PropertyChanges { target: targeticon; height: 50; width: 50 }
            PropertyChanges { target: directionelementtest
                testtext: "The particles should be directed at the rectangle.\n"+
                "Next, let's set an arbritary point to direct the particles to." }
        },
        State { name: "onpoint"; when: statenum == 3
            PropertyChanges { target: emitter; velocity: pointdirectionelement }
            PropertyChanges { target: directionelementtest
                testtext: "The particles should be directed upwards with a small amount of spread.\n"+
                "Next, let's create a fountain with CumulativeDirection and a downward PointDirection" }
        },
        State { name: "cumulative"; when: statenum == 4
            PropertyChanges { target: emitter; emitRate: 200; velocity: cumulativedirectionelement
                acceleration: pointdirectionelementdownward }
            PropertyChanges { target: imgparticle; color: "aqua"; colorVariation: .2 }
            PropertyChanges { target: directionelementtest
                testtext: "The particles should be flowing upwards and falling in a fountain effect.\n"+
                "Advance to restart the test." }
        }
    ]
}