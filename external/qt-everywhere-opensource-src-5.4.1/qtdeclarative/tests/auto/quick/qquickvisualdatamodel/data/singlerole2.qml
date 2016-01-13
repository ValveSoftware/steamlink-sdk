import QtQuick 2.0

ListView {
    width: 100
    height: 100
    model: myModel
    delegate: Component {
        Text { objectName: "name"; text: modelData; function getText() { return modelData } }
    }
}
