import QtQuick 2.0

Rectangle {
    id: root
    width: 500
    height: 600

    // time to pause between each add, remove, etc.
    // (obviously, must be less than 'duration' value to actually test that
    // interrupting transitions will still produce the correct result)
    property int timeBetweenActions: duration / 2

    property int duration: 100

    property int count: grid.count

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            width: 80
            height: 60
            border.width: 1
            Column {
                Text { text: index }
                Text {
                    text: wrapper.x + ", " + wrapper.y
                }
                Text {
                    id: textName
                    objectName: "textName"
                    text: name
                }
            }
            color: GridView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }

    GridView {
        id: grid

        property bool populateDone

        property bool runningAddTargets: false
        property bool runningAddDisplaced: false
        property bool runningMoveTargets: false
        property bool runningMoveDisplaced: false
        property bool runningRemoveTargets: false
        property bool runningRemoveDisplaced: false

        objectName: "grid"
        width: 240
        height: 320
        cellWidth: 80
        cellHeight: 60
        anchors.centerIn: parent
        cacheBuffer: 0
        model: testModel
        delegate: myDelegate

        add: Transition {
            id: addTargets
            enabled: enableAddTransitions
            SequentialAnimation {
                ScriptAction { script: grid.runningAddTargets = true }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; from: addTargets_transitionFrom.x; duration: root.duration }
                    NumberAnimation { properties: "y"; from: addTargets_transitionFrom.y; duration: root.duration }
                }
                ScriptAction { script: grid.runningAddTargets = false }
            }
        }

        addDisplaced: Transition {
            id: addDisplaced
            enabled: enableAddTransitions
            SequentialAnimation {
                ScriptAction { script: grid.runningAddDisplaced = true }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; from: addDisplaced_transitionFrom.x; duration: root.duration }
                    NumberAnimation { properties: "y"; from: addDisplaced_transitionFrom.y; duration: root.duration }
                }
                ScriptAction { script: grid.runningAddDisplaced = false }
            }
        }

        move: Transition {
            id: moveTargets
            enabled: enableMoveTransitions
            SequentialAnimation {
                ScriptAction { script: grid.runningMoveTargets = true }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; from: moveTargets_transitionFrom.x; duration: root.duration }
                    NumberAnimation { properties: "y"; from: moveTargets_transitionFrom.y; duration: root.duration }
                }
                ScriptAction { script: grid.runningMoveTargets = false }
            }
        }

        moveDisplaced: Transition {
            id: moveDisplaced
            enabled: enableMoveTransitions
            SequentialAnimation {
                ScriptAction { script: grid.runningMoveDisplaced = true }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; from: moveDisplaced_transitionFrom.x; duration: root.duration }
                    NumberAnimation { properties: "y"; from: moveDisplaced_transitionFrom.y; duration: root.duration }
                }
                ScriptAction { script: grid.runningMoveDisplaced = false }
            }
        }

        remove: Transition {
            id: removeTargets
            enabled: enableRemoveTransitions
            SequentialAnimation {
                ScriptAction { script: grid.runningRemoveTargets = true }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; to: removeTargets_transitionTo.x; duration: root.duration }
                    NumberAnimation { properties: "y"; to: removeTargets_transitionTo.y; duration: root.duration }
                }
                ScriptAction { script: grid.runningRemoveTargets = false }
            }
        }

        removeDisplaced: Transition {
            id: removeDisplaced
            enabled: enableRemoveTransitions
            SequentialAnimation {
                ScriptAction { script: grid.runningRemoveDisplaced = true }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; from: removeDisplaced_transitionFrom.x; duration: root.duration }
                    NumberAnimation { properties: "y"; from: removeDisplaced_transitionFrom.y; duration: root.duration }
                }
                ScriptAction { script: grid.runningRemoveDisplaced = false }
            }
        }
    }

    Rectangle {
        anchors.fill: grid
        color: "lightsteelblue"
        opacity: 0.2
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: 20; height: 20
        color: "white"
        NumberAnimation on x { loops: Animation.Infinite; from: 0; to: 300; duration: 100000 }
    }
}



