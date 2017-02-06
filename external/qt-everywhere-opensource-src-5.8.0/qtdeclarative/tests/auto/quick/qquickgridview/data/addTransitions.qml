import QtQuick 2.0

Rectangle {
    id: root
    width: 550
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

            onXChanged: checkPos()
            onYChanged: checkPos()

            function checkPos() {
                if (Qt.point(x, y) == targetItems_transitionFrom)
                    model_targetItems_transitionFrom.addItem(name, "")
                if (Qt.point(x, y) == displacedItems_transitionVia)
                    model_displacedItems_transitionVia.addItem(name, "")
            }
        }
    }

    GridView {
        id: grid

        property int targetTransitionsDone
        property int displaceTransitionsDone

        property var targetTrans_items: new Object()
        property var targetTrans_targetIndexes: new Array()
        property var targetTrans_targetItems: new Array()

        property var displacedTrans_items: new Object()
        property var displacedTrans_targetIndexes: new Array()
        property var displacedTrans_targetItems: new Array()

        objectName: "grid"
        width: 240
        height: 320
        cellWidth: 80
        cellHeight: 60
        cacheBuffer: 0
        anchors.centerIn: parent
        model: testModel
        delegate: myDelegate

        // for QQmlListProperty types
        function copyList(propList) {
            var temp = new Array()
            for (var i=0; i<propList.length; i++)
                temp.push(propList[i])
            return temp
        }

        add: Transition {
            id: targetTransition

            SequentialAnimation {
                ScriptAction {
                    script: {
                        grid.targetTrans_items[targetTransition.ViewTransition.item.nameData] = targetTransition.ViewTransition.index
                        grid.targetTrans_targetIndexes.push(targetTransition.ViewTransition.targetIndexes)
                        grid.targetTrans_targetItems.push(grid.copyList(targetTransition.ViewTransition.targetItems))
                    }
                }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; from: targetItems_transitionFrom.x; duration: root.duration }
                    NumberAnimation { properties: "y"; from: targetItems_transitionFrom.y; duration: root.duration }
                }

                ScriptAction { script: grid.targetTransitionsDone += 1 }
            }
        }

        addDisplaced: Transition {
            id: displaced

            SequentialAnimation {
                ScriptAction {
                    script: {
                        grid.displacedTrans_items[displaced.ViewTransition.item.nameData] = displaced.ViewTransition.index
                        grid.displacedTrans_targetIndexes.push(displaced.ViewTransition.targetIndexes)
                        grid.displacedTrans_targetItems.push(grid.copyList(displaced.ViewTransition.targetItems))
                    }
                }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; duration: root.duration; to: displacedItems_transitionVia.x }
                    NumberAnimation { properties: "y"; duration: root.duration; to: displacedItems_transitionVia.y }
                }
                NumberAnimation { properties: "x,y"; duration: root.duration }

                ScriptAction { script: grid.displaceTransitionsDone += 1 }
            }

        }
    }

    Rectangle {
        anchors.fill: grid
        color: "lightsteelblue"
        opacity: 0.2
    }
}


