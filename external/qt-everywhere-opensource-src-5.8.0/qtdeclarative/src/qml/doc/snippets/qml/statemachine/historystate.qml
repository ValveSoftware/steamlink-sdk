/****************************************************************************
**
** Copyright (C) 2014 Ford Motor Company
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

//! [document]
import QtQuick 2.0
import QtQml.StateMachine 1.0 as DSM

Rectangle {
    Button {
        anchors.fill: parent
        id: button
        text: "Press me"
        DSM.StateMachine {
            id: stateMachine
            initialState: parentState
            running: true
            DSM.State {
                id: parentState
                initialState: child2
                onEntered: console.log("parentState entered")
                onExited: console.log("parentState exited")
                DSM.State {
                    id: child1
                    onEntered: console.log("child1 entered")
                    onExited: console.log("child1 exited")
                }
                DSM.State {
                    id: child2
                    onEntered: console.log("child2 entered")
                    onExited: console.log("child2 exited")
                }
                DSM.HistoryState {
                    id: historyState
                    defaultState: child1
                }
                DSM.SignalTransition {
                    targetState: historyState

                    // Clicking the button will cause the state machine to enter the child state
                    // that parentState was in the last time parentState was exited, or the history state's default
                    // state if parentState has never been entered.
                    signal: button.clicked
                }
            }
        }
    }
}
//! [document]
