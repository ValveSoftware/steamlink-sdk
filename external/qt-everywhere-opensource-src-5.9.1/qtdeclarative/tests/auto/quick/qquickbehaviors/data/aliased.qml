import QtQuick 2.3

Rectangle {
    width: 400
    height: 400
    id: rect
    property bool accelerating : false
    
    Text {
        anchors.centerIn: parent
        text: "Press anywere to accelerate"
    }

    Accelerator {
        id: acc
        objectName: "acc"
        anchors.fill: parent
        value: accelerating ? 400 : 0
        Behavior on value {
            NumberAnimation {
                duration: 500
            }
        }
    }

    MouseArea {
        id: clicker
        anchors.fill: parent
    }

    states: State {
        name: "moved"
        when: clicker.pressed
        PropertyChanges {
            target: rect
            accelerating: true
        }
    }
}
