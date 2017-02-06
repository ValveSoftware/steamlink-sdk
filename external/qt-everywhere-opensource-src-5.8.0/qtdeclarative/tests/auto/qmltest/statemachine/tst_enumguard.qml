/****************************************************************************
**
** Copyright (C) 2016 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite module of the Qt Toolkit.
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

import QtTest 1.1
import QtQml.StateMachine 1.0

TestCase {
    id: testCase
    StateMachine {
        id: machine
        initialState: startState
        State {
            id: startState
            SignalTransition {
                id: signalTrans
                signal: testCase.mysignal
                guard: alignment === QState.ParallelStates
                targetState: finalState
            }
        }
        FinalState {
            id: finalState
        }
    }

    SignalSpy {
        id: finalStateActive
        target: finalState
        signalName: "activeChanged"
    }

    signal mysignal(int alignment)

    name: "testEnumGuard"
    function test_enumGuard()
    {
        // Start statemachine, should not have reached finalState yet.
        machine.start()
        tryCompare(finalStateActive, "count", 0)
        tryCompare(machine, "running", true)

        // Emit the signalTrans.signal which will evaluate the guard. The
        // guard should return true, finalState be reached and the
        // statemachine be stopped.
        testCase.mysignal(QState.ParallelStates)
        tryCompare(finalStateActive, "count", 1)
        tryCompare(machine, "running", false)

        // Restart machine.
        machine.start()
        tryCompare(machine, "running", true)
        tryCompare(finalStateActive, "count", 2)

        // Emit signal that makes the signalTrans.guard return false. The
        // finalState should not have been triggered.
        testCase.mysignal(QState.ExclusiveStates)
        tryCompare(finalStateActive, "count", 2)
        tryCompare(machine, "running", true)
    }
}
