/****************************************************************************
**
** Copyright (C) 2014 Ford Motor Company
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
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
                guard: mystr == "test1"
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

    signal mysignal(string mystr, bool mybool, int myint)

    name: "testGuardCondition"
    function test_guardCondition()
    {
        // Start statemachine, should not have reached finalState yet.
        machine.start()
        tryCompare(finalStateActive, "count", 0)
        tryCompare(machine, "running", true)

        // Emit the signalTrans.signal which will evaluate the guard. The
        // guard should return true, finalState be reached and the
        // statemachine be stopped.
        testCase.mysignal("test1", true, 2)
        tryCompare(finalStateActive, "count", 1)
        tryCompare(machine, "running", false)

        // Restart machine.
        machine.start()
        tryCompare(machine, "running", true)
        tryCompare(finalStateActive, "count", 2)

        // Emit signal that makes the signalTrans.guard return false. The
        // finalState should not have been triggered.
        testCase.mysignal("test2", true, 2)
        tryCompare(finalStateActive, "count", 2)
        tryCompare(machine, "running", true)

        // Change the guard in javascript to test that boolean true/false
        // works as expected.
        signalTrans.guard = false;
        testCase.mysignal("test1", true, 2)
        tryCompare(finalStateActive, "count", 2)
        tryCompare(machine, "running", true)
        signalTrans.guard = true;
        testCase.mysignal("test1", true, 2)
        tryCompare(finalStateActive, "count", 3)
        tryCompare(machine, "running", false)
    }
}
