import QtQuick 1.0

Rectangle {
    id: mainrect
    width: 200; height: 200
    state: "first"
    states: [
        State {
            name: "first"
            PropertyChanges {
                target: mainrect
                color: "red"
            }
        },
        State {
            name: "second"
            PropertyChanges {
                target: mainrect
                color: "blue"
            }
        }
    ]
    transitions: [
        Transition {
            from: "first"
            to: "second"
            reversible: true
            SequentialAnimation {
                ColorAnimation {
                    duration: 2000
                    target: mainrect
                    property: "color"
                }
            }
        }
    ]
    MouseArea {
        anchors.fill: parent
        onClicked: { mainrect.state = 'second' }
    }
}
