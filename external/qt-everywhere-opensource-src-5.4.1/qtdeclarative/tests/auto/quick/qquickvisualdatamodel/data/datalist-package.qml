import QtQuick 2.0

ListView {
    width: 100
    height: 100
    model: visualModel.parts.package
    VisualDataModel {
        id: visualModel
        objectName: "visualModel"
        model: myModel
        delegate: Package {
            Rectangle {
                height: 25
                width: 100
                Package.name: "package"
                Text { objectName: "display"; text: display }
            }
        }
    }
}
