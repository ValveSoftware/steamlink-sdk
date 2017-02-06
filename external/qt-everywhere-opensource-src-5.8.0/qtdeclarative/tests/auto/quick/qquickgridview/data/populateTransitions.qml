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
                if (Qt.point(x, y) == transitionFrom)
                    model_transitionFrom.addItem(name, "")
                if (Qt.point(x, y) == transitionVia)
                    model_transitionVia.addItem(name, "")
            }
        }
    }

    GridView {
        id: grid

        property int countPopulateTransitions
        property int countAddTransitions

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

        populate: usePopulateTransition ? popTransition : null

        add: Transition {
            SequentialAnimation {
                ScriptAction { script: grid.countAddTransitions += 1 }
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
            ScriptAction { script: grid.countPopulateTransitions += 1 }
        }
    }

    Rectangle {
        anchors.fill: grid
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


