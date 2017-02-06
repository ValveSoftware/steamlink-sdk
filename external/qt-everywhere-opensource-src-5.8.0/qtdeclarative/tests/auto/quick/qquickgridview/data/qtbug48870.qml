import QtQuick 2.6

Rectangle {
    width: 500
    height: 500
    color: "blue"

    GridView {
        id: view
        objectName: "view"
        anchors.fill: parent
        model: testModel
        cellWidth: 150
        cellHeight: 150
        readonly property int columns: Math.floor(width / cellWidth)

        delegate: Rectangle {
            width: GridView.view.cellWidth
            height: GridView.view.cellHeight
            color: (row & 1) != (col & 1) ? "green" : "red"
            readonly property int row: index / view.columns
            readonly property int col: index % view.columns

            Text {
                anchors.centerIn: parent
                text: "Item " + index
            }
        }
    }
}
