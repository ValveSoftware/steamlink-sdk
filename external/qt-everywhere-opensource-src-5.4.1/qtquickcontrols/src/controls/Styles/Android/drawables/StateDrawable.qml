/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

import QtQuick 2.2

Drawable {
    id: root

    implicitWidth: Math.max(loader.implicitWidth, styleDef.width || 0)
    implicitHeight: Math.max(loader.implicitHeight, styleDef.height || 0)

    property int prevMatch: 0

    DrawableLoader {
        id: loader
        anchors.fill: parent
        visible: !animation.active
        focused: root.focused
        pressed: root.pressed
        checked: root.checked
        selected: root.selected
        accelerated: root.accelerated
        window_focused: root.window_focused
        index: root.index
        level: root.level
        levelId: root.levelId
        orientations: root.orientations
        duration: root.duration
        excludes: root.excludes
        clippables: root.clippables
    }

    Loader {
        id: animation
        property var animDef
        active: false
        anchors.fill: parent
        sourceComponent: AnimationDrawable {
            anchors.fill: parent
            styleDef: animDef
            focused: root.focused
            pressed: root.pressed
            checked: root.checked
            selected: root.selected
            accelerated: root.accelerated
            window_focused: root.window_focused
            index: root.index
            level: root.level
            levelId: root.levelId
            orientations: root.orientations
            duration: root.duration
            excludes: root.excludes
            clippables: root.clippables

            oneshot: true
            onRunningChanged: if (!running) animation.active = false
        }
    }

    onStyleDefChanged: resolveState()
    Component.onCompleted: resolveState()

    // In order to be able to find appropriate transition paths between
    // various states, the following states must be allowed to change in
    // batches. For example, button-like controls could have a transition
    // path from pressed+checked to unpressed+unchecked. We must let both
    // properties change before we try to find the transition path.
    onEnabledChanged: resolver.start()
    onFocusedChanged: resolver.start()
    onPressedChanged: resolver.start()
    onCheckedChanged: resolver.start()
    onSelectedChanged: resolver.start()
    onAcceleratedChanged: resolver.start()
    onWindow_focusedChanged: resolver.start()

    Timer {
        id: resolver
        interval: 15
        onTriggered: resolveState()
    }

    function resolveState () {
        if (styleDef && styleDef.stateslist) {
            var bestMatch = 0
            var highestScore = -1
            var stateslist = styleDef.stateslist
            var transitions = []

            for (var i = 0; i < stateslist.length; ++i) {

                var score = 0
                var state = stateslist[i]

                if (state.transition)
                    transitions.push(i)

                for (var s in state.states) {
                    if (s === "pressed")
                        score += (pressed === state.states[s]) ? 1 : -10
                    if (s === "checked")
                        score += (checked === state.states[s]) ? 1 : -10
                    if (s === "selected")
                        score += (selected === state.states[s]) ? 1 : -10
                    if (s === "focused")
                        score += (focused === state.states[s]) ? 1 : -10
                    if (s === "enabled")
                        score += (enabled === state.states[s]) ? 1 : -1
                    if (s === "window_focused")
                        score += (window_focused === state.states[s]) ? 1 : -1
                    if (s === "accelerated")
                        score += (accelerated === state.states[s]) ? 1 : -1
                }

                if (score > highestScore) {
                    bestMatch = i
                    highestScore = score
                }
            }

            if (prevMatch != bestMatch) {
                for (var t = 0; t < transitions.length; ++t) {
                    var transition = stateslist[transitions[t]].transition
                    if ((transition.from == prevMatch && transition.to == bestMatch) ||
                        (transition.reverse && transition.from == bestMatch && transition.to == prevMatch)) {
                        animation.animDef = stateslist[transitions[t]].drawable
                        animation.active = true
                        break
                    }
                }
                prevMatch = bestMatch
            }

            loader.styleDef = stateslist[bestMatch].drawable
        }
    }
}
