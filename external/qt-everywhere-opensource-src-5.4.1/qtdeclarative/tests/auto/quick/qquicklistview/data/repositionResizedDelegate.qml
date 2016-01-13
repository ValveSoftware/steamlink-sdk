import QtQuick 2.0

ListView {
    id: root

    width: 200; height: 200

    orientation: (testHorizontal == true) ? Qt.Horizontal : Qt.Vertical
    layoutDirection: (testRightToLeft == true) ? Qt.RightToLeft : Qt.LeftToRight
    verticalLayoutDirection: (testBottomToTop == true) ? ListView.BottomToTop : ListView.TopToBottom

    model: VisualItemModel {
        Rectangle {
            objectName: "red"
            width: 200; height: 200; color: "red"
            Text { text: parent.x + ", " + parent.y }
        }
        Grid {
            id: grid
            objectName: "positioner"
            columns: root.orientation == Qt.Vertical ? 1 : 2
            Repeater {
                id: rpt
                model: 1
                Rectangle {
                    width: 120; height: 120; color: "orange"; border.width: 1
                    Column {
                        Text { text: grid.x + ", " + grid.y }
                        Text { text: index }
                    }
                }
            }
        }
        Rectangle {
            objectName: "yellow"
            width: 200; height: 200; color: "yellow"
            Text { text: parent.x + ", " + parent.y }
        }
    }

    focus: true

    function incrementRepeater() {
        rpt.model += 1
    }

    function decrementRepeater() {
        rpt.model -= 1
    }

    Text { anchors.right: parent.right; anchors.bottom: parent.bottom; text: root.contentX + ", " + root.contentY }
}

