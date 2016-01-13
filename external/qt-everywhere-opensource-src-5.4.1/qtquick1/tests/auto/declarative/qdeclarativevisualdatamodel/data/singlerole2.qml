import QtQuick 1.0

ListView {
    width: 100
    height: 100
    anchors.fill: parent
    model: myModel
    delegate: Component {
        Text { objectName: "name"; text: modelData }
    }
}
