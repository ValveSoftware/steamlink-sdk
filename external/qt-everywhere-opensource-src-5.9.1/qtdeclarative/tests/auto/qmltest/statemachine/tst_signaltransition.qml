/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Copyright (C) 2017 The Qt Company
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
                signal: testCase.onMysignal
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

    signal mysignal()

    name: "testSignalTransition"
    function test_signalTransition()
    {
        // Start statemachine, should not have reached finalState yet.
        machine.start()
        tryCompare(finalStateActive, "count", 0)
        tryCompare(machine, "running", true)

        testCase.mysignal()
        tryCompare(finalStateActive, "count", 1)
        tryCompare(machine, "running", false)
    }
}
