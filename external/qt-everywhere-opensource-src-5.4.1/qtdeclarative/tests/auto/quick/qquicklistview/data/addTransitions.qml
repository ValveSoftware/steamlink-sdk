import QtQuick 2.0

Rectangle {
    id: root
    width: 500
    height: 600

    property int duration: 10
    property int count: list.count

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper

            property string nameData: name

            objectName: "wrapper"
            height: 20
            width: 240
            Text { text: index }
            Text {
                x: 30
                id: textName
                objectName: "textName"
                text: name
            }
            Text {
                x: 200
                text: wrapper.y
            }
            color: ListView.isCurrentItem ? "lightsteelblue" : "white"

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

    ListView {
        id: list

        property int targetTransitionsDone
        property int displaceTransitionsDone

        property var targetTrans_items: new Object()
        property var targetTrans_targetIndexes: new Array()
        property var targetTrans_targetItems: new Array()

        property var displacedTrans_items: new Object()
        property var displacedTrans_targetIndexes: new Array()
        property var displacedTrans_targetItems: new Array()

        objectName: "list"
        focus: true
        anchors.centerIn: parent
        width: 240
        height: 320
        cacheBuffer: 60
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
                        list.targetTrans_items[targetTransition.ViewTransition.item.nameData] = targetTransition.ViewTransition.index
                        list.targetTrans_targetIndexes.push(targetTransition.ViewTransition.targetIndexes)
                        list.targetTrans_targetItems.push(list.copyList(targetTransition.ViewTransition.targetItems))
                    }
                }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; from: targetItems_transitionFrom.x; duration: root.duration }
                    NumberAnimation { properties: "y"; from: targetItems_transitionFrom.y; duration: root.duration }
                }

                ScriptAction { script: list.targetTransitionsDone += 1 }
            }
        }

        addDisplaced: Transition {
            id: displaced

            SequentialAnimation {
                ScriptAction {
                    script: {
                        list.displacedTrans_items[displaced.ViewTransition.item.nameData] = displaced.ViewTransition.index
                        list.displacedTrans_targetIndexes.push(displaced.ViewTransition.targetIndexes)
                        list.displacedTrans_targetItems.push(list.copyList(displaced.ViewTransition.targetItems))
                    }
                }
                ParallelAnimation {
                    NumberAnimation { properties: "x"; duration: root.duration; to: displacedItems_transitionVia.x }
                    NumberAnimation { properties: "y"; duration: root.duration; to: displacedItems_transitionVia.y }
                }
                NumberAnimation { properties: "x,y"; duration: root.duration }

                ScriptAction { script: list.displaceTransitionsDone += 1 }
            }

        }
    }

    Rectangle {
        anchors.fill: list
        color: "lightsteelblue"
        opacity: 0.2
    }

    // XXX will it pass without these if I just wait for polish?
    // check all of these tests - if not, then mark this bit with the bug number!
    Rectangle {
        anchors.bottom: parent.bottom
        width: 20; height: 20
        color: "white"
        NumberAnimation on x { loops: Animation.Infinite; from: 0; to: 300; duration: 100000 }
    }
}

