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
                if (Qt.point(x, y) == transitionFrom)
                    model_transitionFrom.addItem(name, "")
                if (Qt.point(x, y) == transitionVia) {
                    model_transitionVia.addItem(name, "")
                }
            }
        }
    }

    ListView {
        id: list

        property int countPopulateTransitions
        property int countAddTransitions

        objectName: "list"
        focus: true
        anchors.centerIn: parent
        width: 240
        height: 320
        cacheBuffer: 60
        model: testModel
        delegate: myDelegate

        populate: usePopulateTransition ? popTransition : null

        add: Transition {
            SequentialAnimation {
                ScriptAction { script: list.countAddTransitions += 1 }
                NumberAnimation { properties: "x,y"; duration: root.duration }
            }
        }
    }

    Transition {
        id: popTransition
        SequentialAnimation {
            ParallelAnimation {
                NumberAnimation { properties: "x"; from: transitionFrom.x; to: transitionVia.x; duration: root.duration }
                NumberAnimation { properties: "y"; from: transitionFrom.y; to: transitionVia.y; duration: root.duration }
            }
            NumberAnimation { properties: "x,y"; duration: root.duration }
            ScriptAction { script: list.countPopulateTransitions += 1 }
        }
    }


    Rectangle {
        anchors.fill: list
        color: "lightsteelblue"
        opacity: 0.2
    }

    Component.onCompleted: {
        if (dynamicallyPopulate) {
            for (var i=0; i<30; i++)
                testModel.addItem("item " + i, "")
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: 20; height: 20
        color: "white"
        NumberAnimation on x { loops: Animation.Infinite; from: 0; to: 300; duration: 100000 }
    }
}


