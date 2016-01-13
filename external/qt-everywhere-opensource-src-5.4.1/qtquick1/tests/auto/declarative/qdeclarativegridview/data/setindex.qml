import QtQuick 1.0

Rectangle {
    width: 200
    height: 200
    Component {
        id: appDelegate

        Item {
            id : wrapper
             function startupFunction() {
                 if (index == 5) view.currentIndex = index;
             }
            Component.onCompleted: startupFunction();
            width: 30; height: 30
            Text { text: index }
        }
    }

    GridView {
        id: view
        objectName: "grid"
        anchors.fill: parent
        cellWidth: 30; cellHeight: 30
        model: 35
        delegate: appDelegate
        focus: true
    }
}
