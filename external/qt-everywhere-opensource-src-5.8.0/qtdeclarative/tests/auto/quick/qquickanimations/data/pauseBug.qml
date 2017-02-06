import QtQuick 2.0

SequentialAnimation {
    id: animation
    running: true
    ScriptAction { script: animation.paused = true }
}
