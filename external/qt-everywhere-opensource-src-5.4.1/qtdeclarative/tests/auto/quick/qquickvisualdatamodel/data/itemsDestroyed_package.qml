import QtQuick 2.0

Item {
    width: 100
    height: 100

    ListView {
        anchors.fill: parent

        model: visualModel.parts.list
    }
    VisualDataModel {
        id: visualModel

        model: myModel
        delegate: Package {
            Item {
                Package.name: "list"
                width: 100
                height: 20
            }

            Item {
                id: gridItem
                Package.name: "grid"
                width: 50
                height: 50
            }
            Rectangle {
                objectName: "delegate"
                parent: gridItem
                width: 20
                height: 20
            }
        }
    }
    GridView {
        anchors.fill: parent

        model: visualModel.parts.grid
    }
}
