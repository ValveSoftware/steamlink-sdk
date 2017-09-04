/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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
