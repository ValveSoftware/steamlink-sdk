import QtQuick 2.0

ListView {
    width: 100
    height: 100
    model: visualModel.parts.package

    VisualDataModel {
        id: visualModel
        objectName: "visualModel"

        groups: [
            VisualDataGroup { id: visibleItems; objectName: "visibleItems"; name: "visible"; includeByDefault: true },
            VisualDataGroup { id: selectedItems; objectName: "selectedItems"; name: "selected" }
        ]

        model: ListModel {
            id: listModel

            ListElement { number: "one" }
            ListElement { number: "two" }
            ListElement { number: "three" }
            ListElement { number: "four" }
        }

        delegate: Package {
            id: delegate

            property variant test1: index
            property variant test2: model.index
            property variant test3: number
            property variant test4: model.number
            property variant test5: modelData
            property variant test6: model.modelData

            function setTest3(arg) { number = arg }
            function setTest4(arg) { model.number = arg }
            function setTest5(arg) { modelData = arg }
            function setTest6(arg) { model.modelData = arg }

            Item {
                objectName: "delegate"

                Package.name: "package"

                width: 100
                height: 2
            }
        }
    }
}
