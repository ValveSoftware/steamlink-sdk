
import QtQuick 2.0

Rectangle {
    id: root
    width: 300; height: 400
    color: "#2200FF00"

    Loader {
        asynchronous: true
        sourceComponent: viewComp
        anchors.fill: parent
    }

    Component {
        id: viewComp
        ListView {
            objectName: "view"
            width: 300; height: 400
            model: 20
            delegate: aDelegate

            highlight: Rectangle { color: "lightsteelblue" }
        }
    }
    // The delegate for each list
    Component {
        id: aDelegate
        Item {
            objectName: "wrapper"
            width: 300
            height: 50
            Text { text: 'Index: ' + index }
        }
    }
}
