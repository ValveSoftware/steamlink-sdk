import QtQuick 2.0

Rectangle {
    id: root
    width: 260
    height: 320

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper
            objectName: "wrapper"
            width: 80
            height: 60
            border.width: 1
            Text { text: index }
            Text {
                x: 30
                text: wrapper.x + ", " + wrapper.y
                font.pixelSize: 12
            }
            Text {
                y: 20
                id: textName
                objectName: "textName"
                text: name
            }
            color: GridView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }

    // the grid is specifically placed inside another item to test a bug where
    // resizing from anchor changes did not update the content pos correctly
    Item {
        anchors.fill: parent

        GridView {
            clip: true
            objectName: "grid"
            anchors.fill: parent
            cellWidth: 80
            cellHeight: 60

            flow: (testTopToBottom == false) ? GridView.LeftToRight : GridView.TopToBottom
            layoutDirection: (testRightToLeft == true) ? Qt.RightToLeft : Qt.LeftToRight
            verticalLayoutDirection: (testBottomToTop == true) ? GridView.BottomToTop : GridView.TopToBottom
            model: testModel
            delegate: myDelegate
        }

    }
}

