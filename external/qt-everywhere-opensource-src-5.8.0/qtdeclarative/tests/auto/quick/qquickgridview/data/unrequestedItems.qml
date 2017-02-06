import QtQuick 2.0

Item {
    width: 240
    height: 320

    Component {
        id: myDelegate

        Package {
            Rectangle {
                id: leftWrapper
                objectName: "wrapper"
                Package.name: "left"
                height: 80
                width: 60
                Column {
                    Text { text: index }
                    Text { text: name }
                    Text { text: leftWrapper.x + ", " + leftWrapper.y }
                }
                color: ListView.isCurrentItem ? "lightsteelblue" : "white"
            }
            Rectangle {
                id: rightWrapper
                objectName: "wrapper"
                Package.name: "right"
                height: 80
                width: 60
                Column {
                    Text { text: index }
                    Text { text: name }
                    Text { text: rightWrapper.x + ", " + rightWrapper.y }
                }
                color: ListView.isCurrentItem ? "lightsteelblue" : "white"
            }
        }

    }

    VisualDataModel {
        id: visualModel

        delegate: myDelegate
        model: testModel
    }

    GridView {
        id: leftList
        objectName: "leftGrid"
        anchors {
            left: parent.left; top: parent.top;
            right: parent.horizontalCenter; bottom: parent.bottom
        }
        model: visualModel.parts.left
        cellWidth: 60
        cellHeight: 80
        cacheBuffer: 0
    }

    GridView {
        id: rightList
        objectName: "rightGrid"
        anchors {
            left: parent.horizontalCenter; top: parent.top;
            right: parent.right; bottom: parent.bottom
        }
        model: visualModel.parts.right
        cellWidth: 60
        cellHeight: 80
        cacheBuffer: 0
    }
}
