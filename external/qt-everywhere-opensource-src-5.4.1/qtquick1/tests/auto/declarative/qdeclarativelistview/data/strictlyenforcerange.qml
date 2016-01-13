import QtQuick 1.0

ListView {
    id: list
    objectName: "list"
    width: 320
    height: 480

    function fillModel() {
        list.model.append({"col": "red"});
        list.currentIndex = list.count-1
        list.model.append({"col": "blue"});
        list.currentIndex = list.count-1
        list.model.append({"col": "green"});
        list.currentIndex = list.count-1
    }

    model: ListModel { id: listModel }  // empty model
    delegate: Rectangle { id: wrapper; objectName: "wrapper"; color: col; width: 300; height: 400 }
    orientation: "Horizontal"
    snapMode: "SnapToItem"
    cacheBuffer: 1000

    preferredHighlightBegin: 10
    preferredHighlightEnd: 10

    highlightRangeMode: "StrictlyEnforceRange"
    focus: true
}
