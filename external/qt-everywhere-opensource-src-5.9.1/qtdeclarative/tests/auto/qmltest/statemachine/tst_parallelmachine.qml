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
    StateMachine {
        id: myStateMachine
        childMode: State.ParallelStates
        State {
            id: childState1
            childMode: State.ParallelStates
            State {
                id: childState11
            }
            State {
                id: childState12
            }
        }
        State {
            id: childState2
            initialState: childState21
            State {
                id: childState21
            }
            State {
                id: childState22
            }
        }
    }
    name: "nestedParallelMachineStates"

    function test_nestedInitalStates() {
        // uncomment me after vm problems are fixed.
        //            compare(myStateMachine.running, false);
        compare(childState1.active, false);
        compare(childState11.active, false);
        compare(childState12.active, false);
        compare(childState2.active, false);
        compare(childState21.active, false);
        compare(childState22.active, false);
        myStateMachine.start();
        tryCompare(myStateMachine, "running", true);
        tryCompare(childState1, "active", true);
        tryCompare(childState11, "active", true);
        tryCompare(childState12, "active", true);
        tryCompare(childState2, "active", true);
        tryCompare(childState21, "active", true);
        tryCompare(childState22, "active", false);
    }
}
