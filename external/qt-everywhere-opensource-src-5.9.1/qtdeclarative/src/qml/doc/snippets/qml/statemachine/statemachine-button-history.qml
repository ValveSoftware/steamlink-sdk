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
import QtQml.StateMachine 1.0

Rectangle {
//![0]
    Row {
        anchors.fill: parent
        spacing: 2
        Button {
            id: button
            // change the button label to the active state id
            text: s11.active ? "s11" : s12.active ? "s12" :  s13.active ? "s13" : "s3"
        }
        Button {
            id: interruptButton
            text: s1.active ? "Interrupt" : "Resume"
        }
        Button {
            id: quitButton
            text: "quit"
        }
    }

    StateMachine {
        id: stateMachine
        // set the initial state
        initialState: s1

        // start the state machine
        running: true

        State {
            id: s1
            // set the initial state
            initialState: s11

            // create a transition from s1 to s2 when the button is clicked
            SignalTransition {
                targetState: s2
                signal: quitButton.clicked
            }
            // do something when the state enters/exits
            onEntered: console.log("s1 entered")
            onExited: console.log("s1 exited")
            State {
                id: s11
                // create a transition from s1 to s2 when the button is clicked
                SignalTransition {
                    targetState: s12
                    signal: button.clicked
                }
                // do something when the state enters/exits
                onEntered: console.log("s11 entered")
                onExited: console.log("s11 exited")
            }

            State {
                id: s12
                // create a transition from s2 to s3 when the button is clicked
                SignalTransition {
                    targetState: s13
                    signal: button.clicked
                }
                // do something when the state enters/exits
                onEntered: console.log("s12 entered")
                onExited: console.log("s12 exited")
            }
            State {
                id: s13
                // create a transition from s3 to s1 when the button is clicked
                SignalTransition {
                    targetState: s1
                    signal: button.clicked
                }
                // do something when the state enters/exits
                onEntered: console.log("s13 entered")
                onExited: console.log("s13 exited")
            }

            // create a transition from s1 to s3 when the button is clicked
            SignalTransition {
                targetState: s3
                signal: interruptButton.clicked
            }
            HistoryState {
                id: s1h
            }
        }
        FinalState {
            id: s2
            onEntered: console.log("s2 entered")
            onExited: console.log("s2 exited")
        }
        State {
            id: s3
            SignalTransition {
                targetState: s1h
                signal: interruptButton.clicked
            }
            // do something when the state enters/exits
            onEntered: console.log("s3 entered")
            onExited: console.log("s3 exited")
        }
        onFinished: Qt.quit()
    }
//![0]
}
//! [document]
