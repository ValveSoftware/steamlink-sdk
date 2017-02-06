import QtQuick 2.0

Item {
    id: root
    property bool currentItemIsNull: view.currentItem === null

    PathView {
        id: view
        objectName: "view"
        model: testModel
        delegate: Item {}

        path: Path {
            PathLine {
                y: 0
                x: 0
            }
        }
    }
}
