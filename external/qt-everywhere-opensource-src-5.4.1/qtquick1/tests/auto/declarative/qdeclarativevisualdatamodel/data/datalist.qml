import QtQuick 1.0

ListView {
    width: 100
    height: 100
    anchors.fill: parent
    model: VisualDataModel {
        id: visualModel
        objectName: "visualModel"
        model: myModel
        delegate: Component {
            Rectangle {
                height: 25
                width: 100
                Text { objectName: "display"; text: display }
            }
        }
    }
}
