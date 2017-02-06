import QtQuick 2.0

Rectangle {
    id: root
    width: 500
    height: 500

    property int duration: 50

    property real incrementalSize: 5

    property int populateTransitionsDone
    property int addTransitionsDone
    property int displaceTransitionsDone

    property var targetTrans_items: new Object()
    property var targetTrans_targetIndexes: new Array()
    property var targetTrans_targetItems: new Array()

    property var displacedTrans_items: new Object()
    property var displacedTrans_targetIndexes: new Array()
    property var displacedTrans_targetItems: new Array()

    // for QQmlListProperty types
    function copyList(propList) {
        var temp = new Array()
        for (var i=0; i<propList.length; i++)
            temp.push(propList[i])
        return temp
    }

    function checkPos(x, y, name) {
        if (Qt.point(x, y) == targetItems_transitionFrom)
            model_targetItems_transitionFrom.addItem(name, "")
        if (Qt.point(x, y) == displacedItems_transitionVia)
            model_displacedItems_transitionVia.addItem(name, "")
    }

    Component.onCompleted: {
        if (dynamicallyPopulate) {
            for (var i=0; i<30; i++)
                testModel.addItem("item " + i, "")
        }
    }

    Transition {
        id: populateTransition
        enabled: usePopulateTransition

        SequentialAnimation {
            ScriptAction {
                script: {
                    root.targetTrans_items[populateTransition.ViewTransition.item.nameData] = populateTransition.ViewTransition.index
                    root.targetTrans_targetIndexes.push(populateTransition.ViewTransition.targetIndexes)
                    root.targetTrans_targetItems.push(root.copyList(populateTransition.ViewTransition.targetItems))
                }
            }
            ParallelAnimation {
                NumberAnimation { properties: "x"; from: targetItems_transitionFrom.x; duration: root.duration }
                NumberAnimation { properties: "y"; from: targetItems_transitionFrom.y; duration: root.duration }
            }

            ScriptAction { script: root.populateTransitionsDone += 1 }
        }
    }

    Transition {
        id: addTransition
        enabled: enableAddTransition

        SequentialAnimation {
            ScriptAction {
                script: {
                    root.targetTrans_items[addTransition.ViewTransition.item.nameData] = addTransition.ViewTransition.index
                    root.targetTrans_targetIndexes.push(addTransition.ViewTransition.targetIndexes)
                    root.targetTrans_targetItems.push(root.copyList(addTransition.ViewTransition.targetItems))
                }
            }
            ParallelAnimation {
                NumberAnimation { properties: "x"; from: targetItems_transitionFrom.x; duration: root.duration }
                NumberAnimation { properties: "y"; from: targetItems_transitionFrom.y; duration: root.duration }
            }

            ScriptAction { script: root.addTransitionsDone += 1 }
        }
    }

    Transition {
        id: displaced

        SequentialAnimation {
            ScriptAction {
                script: {
                    root.displacedTrans_items[displaced.ViewTransition.item.nameData] = displaced.ViewTransition.index
                    root.displacedTrans_targetIndexes.push(displaced.ViewTransition.targetIndexes)
                    root.displacedTrans_targetItems.push(root.copyList(displaced.ViewTransition.targetItems))
                }
            }
            ParallelAnimation {
                NumberAnimation { properties: "x"; duration: root.duration; to: displacedItems_transitionVia.x }
                NumberAnimation { properties: "y"; duration: root.duration; to: displacedItems_transitionVia.y }
            }
            NumberAnimation { properties: "x,y"; duration: root.duration }

            ScriptAction { script: root.displaceTransitionsDone += 1 }
        }

    }

    Row {
        objectName: "row"

        property int count: children.length - 1 // omit Repeater

        x: 50; y: 50
        width: 400; height: 400
        Repeater {
            objectName: "repeater"
            model: testedPositioner == "row" ? testModel : undefined
            Rectangle {
                property string nameData: name
                objectName: "wrapper"
                width: 30 + index*root.incrementalSize
                height: 30 + index*root.incrementalSize
                border.width: 1
                Column {
                    Text { text: index }
                    Text { objectName: "name"; text: name }
                    Text { text: parent.parent.y }
                }
                onXChanged: root.checkPos(x, y, name)
                onYChanged: root.checkPos(x, y, name)
            }
        }

        populate: populateTransition
        add: addTransition
        move: displaced
    }

    Column {
        objectName: "column"

        property int count: children.length - 1 // omit Repeater

        x: 50; y: 50
        width: 400; height: 400
        Repeater {
            objectName: "repeater"
            model: testedPositioner == "column" ? testModel : undefined
            Rectangle {
                property string nameData: name
                objectName: "wrapper"
                width: 30 + index*root.incrementalSize
                height: 30 + index*root.incrementalSize
                border.width: 1
                Column {
                    Text { text: index }
                    Text { objectName: "name"; text: name }
                    Text { text: parent.parent.y }
                }
                onXChanged: root.checkPos(x, y, name)
                onYChanged: root.checkPos(x, y, name)
            }
        }

        populate: populateTransition
        add: addTransition
        move: displaced
    }

    Grid {
        objectName: "grid"

        property int count: children.length - 1 // omit Repeater

        x: 50; y: 50
        width: 400; height: 400
        Repeater {
            objectName: "repeater"
            model: testedPositioner == "grid" ? testModel : undefined
            Rectangle {
                property string nameData: name
                objectName: "wrapper"
                width: 30 + index*root.incrementalSize
                height: 30 + index*root.incrementalSize
                border.width: 1
                Column {
                    Text { text: index }
                    Text { objectName: "name"; text: name }
                    Text { text: parent.parent.y }
                }

                onXChanged: root.checkPos(x, y, name)
                onYChanged: root.checkPos(x, y, name)
            }
        }

        populate: populateTransition
        add: addTransition
        move: displaced
    }

    Flow {
        objectName: "flow"

        property int count: children.length - 1 // omit Repeater

        x: 50; y: 50
        width: 400; height: 400
        Repeater {
            objectName: "repeater"
            model: testedPositioner == "flow" ? testModel : undefined
            Rectangle {
                property string nameData: name
                objectName: "wrapper"
                width: 30 + index*root.incrementalSize
                height: 30 + index*root.incrementalSize
                border.width: 1
                Column {
                    Text { text: index }
                    Text { objectName: "name"; text: name }
                    Text { text: parent.parent.x + " " + parent.parent.y }
                }
                onXChanged: root.checkPos(x, y, name)
                onYChanged: root.checkPos(x, y, name)
            }
        }

        populate: populateTransition
        add: addTransition
        move: displaced
    }
}

