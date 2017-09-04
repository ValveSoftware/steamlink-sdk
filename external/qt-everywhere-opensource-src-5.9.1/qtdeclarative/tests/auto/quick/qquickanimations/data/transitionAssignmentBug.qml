import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    property bool nullObject
    Component.onCompleted: nullObject = transitions.length > 0 && transitions[0] === null

    property list<Transition> myTransitions: [Transition {}, Transition {}]
    transitions: myTransitions
}
