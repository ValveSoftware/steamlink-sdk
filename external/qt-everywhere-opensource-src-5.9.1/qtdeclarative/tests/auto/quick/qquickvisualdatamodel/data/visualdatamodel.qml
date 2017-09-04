import QtQuick 2.0

VisualDataModel {
    function setRoot() {
        rootIndex = modelIndex(0);
    }
    function setRootToParent() {
        rootIndex = parentModelIndex();
    }
    model: myModel
    delegate: Item { property bool modelChildren: hasModelChildren }
}
