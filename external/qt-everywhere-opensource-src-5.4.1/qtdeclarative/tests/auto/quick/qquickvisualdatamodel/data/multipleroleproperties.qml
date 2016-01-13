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

        model: StandardItemModel {
            StandardItem { text: "Row 1 Item" }
            StandardItem { text: "Row 2 Item" }
            StandardItem { text: "Row 3 Item" }
            StandardItem { text: "Row 4 Item" }
        }

        delegate: Item {
            id: delegate

            objectName: "delegate"

            property variant test1: index
            property variant test2: model.index
            property variant test3: display
            property variant test4: model.display

            function setTest3(arg) { display = arg }
            function setTest4(arg) { model.display = arg }

            width: 100
            height: 2
        }
    }
}

