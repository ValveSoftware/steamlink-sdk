import QtQuick 2.0

Rectangle {
    id: root

    width: 240
    height: 320

    property int createdIndex: -1

    Component {
        id: myDelegate

        Rectangle {
            id: wrapper
            width: 240; height: 20
            objectName: "wrapper"

            Text { text: index }

            Component.onCompleted: {
                root.createdIndex = index
                ListView.view.model.removeItem(index)
            }
        }
    }

    ListView {
        id: list
        objectName: "list"
        focus: true
        width: 240
        height: 320
        delegate: myDelegate
        model: testModel
    }
}
