import QtQuick 1.0

ListView {
    model: myModel
    delegate: Item {
        objectName: "delegate"
        property variant test1: display
        property variant test2: model.display
        property variant test3: modelData
        property variant test4: model.modelData
        property variant test5: modelData.display
        property variant test6: model
        property variant test7: index
        property variant test8: model.index
        property variant test9: model.modelData.display
    }
}
