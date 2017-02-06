import QtQuick 2.0

Item {
    property bool actionTriggered: false
    property bool stateChangeScriptTriggered: false

    states: State {
        name: "state1"
        StateChangeScript { script: stateChangeScriptTriggered = true }
    }

    transitions: Transition {
        ScriptAction { script: actionTriggered = true }
    }

    Component.onCompleted: state = "state1"
}
