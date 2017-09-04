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
    Button {
        anchors.fill: parent
        id: button

        // change the button label to the active state id
        text: s1.active ? "s1" : s2.active ? "s2" : "s3"
    }

    StateMachine {
        id: stateMachine
        // set the initial state
        initialState: s1

        // start the state machine
        running: true

        State {
            id: s1
            // create a transition from s1 to s2 when the button is clicked
            SignalTransition {
                targetState: s2
                signal: button.clicked
            }
            // do something when the state enters/exits
            onEntered: console.log("s1 entered")
            onExited: console.log("s1 exited")
        }

        State {
            id: s2
            // create a transition from s2 to s3 when the button is clicked
            SignalTransition {
                targetState: s3
                signal: button.clicked
            }
            // do something when the state enters/exits
            onEntered: console.log("s2 entered")
            onExited: console.log("s2 exited")
        }
        State {
            id: s3
            // create a transition from s3 to s1 when the button is clicked
            SignalTransition {
                targetState: s1
                signal: button.clicked
            }
            // do something when the state enters/exits
            onEntered: console.log("s3 entered")
            onExited: console.log("s3 exited")
        }
    }
//![0]
}
//! [document]
