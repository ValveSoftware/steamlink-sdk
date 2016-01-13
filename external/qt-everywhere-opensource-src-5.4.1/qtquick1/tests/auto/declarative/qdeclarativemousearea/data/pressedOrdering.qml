import QtQuick 1.0

Item {
    id: root
    property string value: "base"

    MouseArea {
        id: mouseArea
        width: 200; height: 200
        onClicked: toggleState.state = "toggled"
    }

    StateGroup {
        states: State {
            name: "pressed"
            when: mouseArea.pressed
            PropertyChanges { target: root; value: "pressed" }
        }
    }

    StateGroup {
        id: toggleState
        states: State {
            name: "toggled"
            PropertyChanges { target: root; value: "toggled" }
        }
    }
}
