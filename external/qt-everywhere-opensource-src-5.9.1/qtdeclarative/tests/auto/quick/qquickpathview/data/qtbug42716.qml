import QtQuick 2.0

Rectangle {
    //Note that this file was originally the manual reproduction, MouseAreas were left in.
    id: qmlBrowser

    width: 500
    height: 350

    ListModel {
        id: myModel
        ListElement {
            name: "Bill Jones 0"
        }
        ListElement {
            name: "Jane Doe 1"
        }
        ListElement {
            name: "John Smith 2"
        }
        ListElement {
            name: "Bill Jones 3"
        }
        ListElement {
            name: "Jane Doe 4"
        }
        ListElement {
            name: "John Smith 5"
        }
        ListElement {
            name: "John Smith 6"
        }
        ListElement {
            name: "John Smith 7"
        }
    }

    Component {
        id: delegate

        Text {
            id: nameText
            height: 33
            width: parent.width
            objectName: "delegate"+index

            text: "index: " + index + "   text: " + name
            font.pointSize: 16
            color: PathView.isCurrentItem ? "red" : "black"
        }
    }

    PathView {
        id: contentList
        objectName: "pathView"
        anchors.fill: parent

        property int maxPathItemCount: 7
        property real itemHeight: 34

        delegate: delegate
        model: myModel
        currentIndex: 5
        pathItemCount: maxPathItemCount
        highlightMoveDuration: 0

        path: Path {
            startX: 30 + contentList.width / 2
            startY: 30
            PathLine {
                relativeX: 0
                relativeY: contentList.itemHeight * contentList.maxPathItemCount
            }
        }

        focus: true
        Keys.onLeftPressed: decrementCurrentIndex()
        Keys.onRightPressed: incrementCurrentIndex()
    }

    Column {
        anchors.right: parent.right
        Text {
            text: "Go 1"
            font.weight: Font.Bold
            font.pixelSize: 24
            MouseArea {
                anchors.fill: parent
                onClicked:  contentList.offset = 2.55882
            }
        }
        Text {
            text: "Go 2"
            font.weight: Font.Bold
            font.pixelSize: 24
            MouseArea {
                anchors.fill: parent
                onClicked:  contentList.offset = 0.0882353
            }
        }
        Text {
            text: "Go 3"
            font.weight: Font.Bold
            font.pixelSize: 24
            MouseArea {
                anchors.fill: parent
                onClicked:  contentList.offset = 0.99987
            }
        }
    }
}
