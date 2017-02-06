import QtQuick 2.0

Rectangle {
    id: root
    width: 500
    height: 600

    property int duration: 10
    property int count: grid.count

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper

            property string nameData: name

            objectName: "wrapper"
            width: 80
            height: 60
            Text {
                text: index
            }
            Text {
                x: 40
                text: wrapper.x + ", " + wrapper.y
            }
            Text {
                y: 20
                id: textName
                objectName: "textName"
                text: name
            }
            Text {
                y: 40
                id: textNumber
                objectName: "textNumber"
                text: number
            }
            color: GridView.isCurrentItem ? "lightsteelblue" : "white"
            border.width: 1

            onXChanged: checkPos()
            onYChanged: checkPos()

            function checkPos() {
                if (Qt.point(x, y) == displaced_transitionVia)
                    model_displaced_transitionVia.addItem(name, "")
                if (Qt.point(x, y) == addDisplaced_transitionVia)
                    model_addDisplaced_transitionVia.addItem(name, "")
                if (Qt.point(x, y) == moveDisplaced_transitionVia)
                    model_moveDisplaced_transitionVia.addItem(name, "")
                if (Qt.point(x, y) == removeDisplaced_transitionVia)
                    model_removeDisplaced_transitionVia.addItem(name, "")
            }
        }
    }

    GridView {
        id: grid

        property int targetTransitionsDone
        property int displaceTransitionsDone

        property var displacedTargetIndexes: new Array()
        property var displacedTargetItems: new Array()

        // for QQmlListProperty types
        function copyList(propList) {
            var temp = new Array()
            for (var i=0; i<propList.length; i++)
                temp.push(propList[i])
            return temp
        }

        objectName: "grid"
        focus: true
        anchors.centerIn: parent
        width: 240
        height: 320
        cellWidth: 80
        cellHeight: 60
        cacheBuffer: 0
        model: testModel
        delegate: myDelegate

        displaced: useDisplaced ? displaced : null
        addDisplaced: useAddDisplaced ? addDisplaced : null
        moveDisplaced: useMoveDisplaced ? moveDisplaced : null
        removeDisplaced: useRemoveDisplaced ? removeDisplaced : null

        Transition {
            id: displaced
            enabled: displacedEnabled
            SequentialAnimation {
                ScriptAction {
                    script: {
                        grid.displacedTargetIndexes.push(displaced.ViewTransition.targetIndexes)
                        grid.displacedTargetItems.push(grid.copyList(displaced.ViewTransition.targetItems))
                    }
                }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; to: displaced_transitionVia.x; duration: root.duration }
                    NumberAnimation { properties: "y"; to: displaced_transitionVia.y; duration: root.duration }
                }
                NumberAnimation { properties: "x,y"; duration: root.duration }
                PropertyAction { target: grid; property: "displaceTransitionsDone"; value: true }
            }
        }

        Transition {
            id: addDisplaced
            enabled: addDisplacedEnabled
            SequentialAnimation {
                ScriptAction {
                    script: {
                        grid.displacedTargetIndexes.push(addDisplaced.ViewTransition.targetIndexes)
                        grid.displacedTargetItems.push(grid.copyList(addDisplaced.ViewTransition.targetItems))
                    }
                }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; to: addDisplaced_transitionVia.x; duration: root.duration }
                    NumberAnimation { properties: "y"; to: addDisplaced_transitionVia.y; duration: root.duration }
                }
                NumberAnimation { properties: "x,y"; duration: root.duration }
                PropertyAction { target: grid; property: "displaceTransitionsDone"; value: true }
            }
        }

        Transition {
            id: moveDisplaced
            enabled: moveDisplacedEnabled
            SequentialAnimation {
                ScriptAction {
                    script: {
                        grid.displacedTargetIndexes.push(moveDisplaced.ViewTransition.targetIndexes)
                        grid.displacedTargetItems.push(grid.copyList(moveDisplaced.ViewTransition.targetItems))
                    }
                }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; to: moveDisplaced_transitionVia.x; duration: root.duration }
                    NumberAnimation { properties: "y"; to: moveDisplaced_transitionVia.y; duration: root.duration }
                }
                NumberAnimation { properties: "x,y"; duration: root.duration }
                PropertyAction { target: grid; property: "displaceTransitionsDone"; value: true }
            }
        }

        Transition {
            id: removeDisplaced
            enabled: removeDisplacedEnabled
            SequentialAnimation {
                ScriptAction {
                    script: {
                        grid.displacedTargetIndexes.push(removeDisplaced.ViewTransition.targetIndexes)
                        grid.displacedTargetItems.push(grid.copyList(removeDisplaced.ViewTransition.targetItems))
                    }
                }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; to: removeDisplaced_transitionVia.x; duration: root.duration }
                    NumberAnimation { properties: "y"; to: removeDisplaced_transitionVia.y; duration: root.duration }
                }
                NumberAnimation { properties: "x,y"; duration: root.duration }
                PropertyAction { target: grid; property: "displaceTransitionsDone"; value: true }
            }
        }
    }

    Rectangle {
        anchors.fill: grid
        color: "lightsteelblue"
        opacity: 0.2
    }
}

