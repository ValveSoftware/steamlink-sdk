import QtQuick 2.0

Rectangle {
    width: 180; height: 200;

    Component {
        id: delegate
        Rectangle {
            id: wrapper
            width: 180; height: 200
            color: "blue"

            states: State {
                name: "otherState"
                PropertyChanges { target: wrapper; color: "green" }
            }

            transitions: Transition {
                PropertyAction { target: wrapper; property: "ListView.delayRemove"; value: true }
                ScriptAction { script: console.log(wrapper.ListView.delayRemove ? "on" : "off") }
            }

            Component.onCompleted: {
                console.log(ListView.delayRemove ? "on" : "off");
                wrapper.state = "otherState"
            }
        }
    }

    ListView {
        model: 1
        delegate: delegate
    }
}
