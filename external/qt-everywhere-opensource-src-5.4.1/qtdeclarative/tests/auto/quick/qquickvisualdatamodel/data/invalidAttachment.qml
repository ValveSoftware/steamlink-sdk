import QtQuick 2.0

Item {
    property VisualDataModel invalidVdm: VisualDataModel.model
    Repeater {
        model: 1
        delegate: Item {
            id: outer
            objectName: "delegate"

            property VisualDataModel validVdm: outer.VisualDataModel.model
            property VisualDataModel invalidVdm: inner.VisualDataModel.model

            Item {
                id: inner
            }
        }
    }
}
