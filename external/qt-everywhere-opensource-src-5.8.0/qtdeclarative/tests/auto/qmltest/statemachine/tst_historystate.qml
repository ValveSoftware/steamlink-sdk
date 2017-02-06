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
        id: stateMachine
        initialState: historyState1

        State {
            id: state1
            SignalTransition {
                id: st1
                targetState: state2
            }
        }

        State {
            id: state2
            initialState: historyState2
            HistoryState {
                id: historyState2
                defaultState: state21
            }
            State {
                id: state21
            }
        }

        HistoryState {
            id: historyState1
            defaultState: state1
        }
    }

    SignalSpy {
        id: state1SpyActive
        target: state1
        signalName: "activeChanged"
    }

    SignalSpy {
        id: state2SpyActive
        target: state2
        signalName: "activeChanged"
    }


    function test_historyStateAsInitialState()
    {
        stateMachine.start();
        tryCompare(stateMachine, "running", true);
        tryCompare(state1SpyActive, "count" , 1);
        tryCompare(state2SpyActive, "count" , 0);
        st1.invoke();
        tryCompare(state1SpyActive, "count" , 2);
        tryCompare(state2SpyActive, "count" , 1);
        tryCompare(state21, "active", true);
        tryCompare(state1, "active", false);
    }
}
