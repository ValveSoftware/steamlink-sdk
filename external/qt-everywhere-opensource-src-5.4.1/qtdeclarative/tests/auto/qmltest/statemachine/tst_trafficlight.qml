/****************************************************************************
**
** Copyright (C) 2014 Ford Motor Company
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite module of the Qt Toolkit.
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

import QtTest 1.1
import QtQml.StateMachine 1.0

TestCase {
    StateMachine {
        id: machine
        initialState: red
        FinalState {
            id: finalState
        }

        State {
            id: red
            initialState: justRed
            State {
                id: justRed
                SignalTransition {
                    id: e1
                    targetState: waitingForGreen
                }
                SignalTransition {
                    id: finalSignal
                    targetState: finalState
                }
            }
            State {
                id: waitingForGreen
                TimeoutTransition {
                    id: e2
                    targetState: yellowred
                    timeout: 30
                }
            }
        }
        State {
            id: yellowred
            TimeoutTransition {
                id: e3
                targetState: green
                timeout: 10
            }
        }
        State {
            id: green
            TimeoutTransition {
                id: e4
                targetState: yellow
                timeout: 50
            }
        }
        State {
            id: yellow
            TimeoutTransition {
                id: e5
                targetState: red
                timeout: 10
            }
        }
    }

    SignalSpy {
        id: machineSpyRunning
        target: machine
        signalName: "runningChanged"
    }

    SignalSpy {
        id: redSpyActive
        target: red
        signalName: "activeChanged"
    }

    SignalSpy {
        id: yellowredSpyActive
        target: yellowred
        signalName: "activeChanged"
    }

    SignalSpy {
        id: greenSpyActive
        target: green
        signalName: "activeChanged"
    }

    SignalSpy {
        id: yellowSpyActive
        target: yellow
        signalName: "activeChanged"
    }


    name: "testTrafficLight"
    function test_trafficLight()
    {
        var i = 1;
        machine.start();
        tryCompare(machine, "running", true);
        tryCompare(machineSpyRunning, "count", 1);
        tryCompare(redSpyActive, "count", 1);
        for (; i <= 5; ++i) {
            e1.invoke();
            tryCompare(yellowredSpyActive, "count", i * 2);
            tryCompare(greenSpyActive, "count", i * 2);
            tryCompare(redSpyActive, "count", i * 2 + 1);
            tryCompare(yellowSpyActive, "count", i * 2);
        }
        finalSignal.guard = false;
        finalSignal.invoke();
        wait(100);
        tryCompare(machine, "running", true);
        finalSignal.guard = true;
        finalSignal.invoke();
        tryCompare(machine, "running", false);
        tryCompare(redSpyActive, "count", i * 2);
    }
}
