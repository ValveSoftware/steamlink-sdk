
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
        GridView {
            objectName: "view"
            width: 300; height: 400
            model: 40
            delegate: aDelegate

            highlight: Rectangle { color: "lightsteelblue" }
        }
    }
    // The delegate for each list
    Component {
        id: aDelegate
        Item {
            objectName: "wrapper"
            width: 100
            height: 100
            Text { text: 'Index: ' + index }
        }
    }
}
