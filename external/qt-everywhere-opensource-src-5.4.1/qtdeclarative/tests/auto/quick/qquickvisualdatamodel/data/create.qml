import QtQuick 2.0

ListView {
    width: 200
    height: 200

    property var persistentHandle

    model: VisualDataModel {
        id: visualModel

        persistedItems.includeByDefault: true

        model: myModel
        delegate: Item {
            id: delegate
            objectName: "delegate"
            width: 200
            height: 20
        }
    }
}
