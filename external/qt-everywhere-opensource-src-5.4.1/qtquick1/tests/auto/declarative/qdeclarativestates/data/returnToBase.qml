import QtQuick 1.0

Rectangle {
    id: theRect
    property bool triggerState: false
    property string stateString: ""
    states: [ State {
        when: triggerState
        PropertyChanges {
            target: theRect
            stateString: "inState"
        }
    },
    State {
        name: ""
        PropertyChanges {
            target: theRect
            stateString: "originalState"
        }
    }]
}
