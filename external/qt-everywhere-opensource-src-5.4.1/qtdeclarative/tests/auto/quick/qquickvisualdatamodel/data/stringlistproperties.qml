import QtQuick 2.0
import tst_qquickvisualdatamodel 1.0

ListView {
    width: 100
    height: 100
    model: VisualDataModel {
        id: visualModel
        objectName: "visualModel"

        groups: [
            VisualDataGroup { id: visibleItems; objectName: "visibleItems"; name: "visible"; includeByDefault: true },
            VisualDataGroup { id: selectedItems; objectName: "selectedItems"; name: "selected" }
        ]

        model: [
            "one",
            "two",
            "three",
            "four"
        ]

        delegate: Item {
            id: delegate

            objectName: "delegate"

            property variant test1: index
            property variant test2: model.index
            property variant test3: modelData
            property variant test4: model.modelData

            function setTest3(arg) { modelData = arg }
            function setTest4(arg) { model.modelData = arg }

            width: 100
            height: 2
        }
    }
}
