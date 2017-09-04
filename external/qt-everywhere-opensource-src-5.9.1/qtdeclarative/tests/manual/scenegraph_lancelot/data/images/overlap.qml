import QtQuick 2.0

Image {
    width: 320
    height: 480
    source: "../shared/winter.png"
    Grid {
        columns: 2
        Item { width: 35; height: 50 }
        Row {
            id: row_0000
            Repeater {
                model: 7
                Rectangle {
                    width: 35
                    height: 50
                    color: Qt.rgba(index & 1 ? 0.75 : 0.85, 1, 1, 0.75)
                    Text { font.pointSize: 8; anchors.centerIn: parent; text: "Col " + (index + 1) }
                }
            }
        }
        Column {
            Repeater {
                model: 8
                Rectangle {
                    width: 35
                    height: 50
                    color: Qt.rgba(1, 1, index & 1 ? 0.75 : 0.85, 0.75)
                    Text { font.pointSize: 8; anchors.centerIn: parent; text: "Row " + (index + 1) }
                }
            }
        }
        Grid {
            id: grid_0001
            columns: 7
            rows: 8
            opacity: 0.5
            Repeater {
                id: repeater_0001
                model: 7 * 8
                onActiveFocusChanged:  console.debug("changed")

                Rectangle {
                    id: rect_0001
                    width: 35
                    height: 50
                    radius: 10
                    gradient: Gradient {
                        GradientStop { position: 0; color: "white" }
                        GradientStop { position: 1; color: "blue" }
                    }
                    border.width: 2
                    border.color: "black"
                }
            }
        }
    }
}
