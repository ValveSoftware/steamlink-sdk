import QtQuick 2.0

ListView {
    width: 100
    height: 100

    function contains(array, value) {
        for (var i = 0; i < array.length; ++i)
            if (array[i] == value)
                return true
        return false
    }

    model: visualModel
    VisualDataModel {
        id: visualModel

        objectName: "visualModel"

        groups: [
            VisualDataGroup { id: visibleItems; objectName: "visibleItems"; name: "visible"; includeByDefault: true },
            VisualDataGroup { id: selectedItems; objectName: "selectedItems"; name: "selected" }
        ]

        model: myModel
        delegate: Item {
            id: delegate

            objectName: "delegate"
            width: 100
            height: 2
            property variant test1: name
            property variant test2: index
            property variant test3: VisualDataModel.itemsIndex
            property variant test4: VisualDataModel.inItems
            property variant test5: VisualDataModel.visibleIndex
            property variant test6: VisualDataModel.inVisible
            property variant test7: VisualDataModel.selectedIndex
            property variant test8: VisualDataModel.inSelected
            property variant test9: VisualDataModel.groups

            function hide() { VisualDataModel.inVisible = false }
            function select() { VisualDataModel.inSelected = true }
        }
    }
}
