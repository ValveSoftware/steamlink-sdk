import QtQuick 1.0

GridView {
    id: view
    width: 240; height: 320

    property variant addedDelegates: []
    property int removedDelegateCount

    model: testModel

    cellWidth: delegateWidth; cellHeight: delegateHeight

    delegate: Rectangle {
        width: delegateWidth; height: delegateHeight
        border.width: 1
        GridView.onAdd: {
            var obj = GridView.view.addedDelegates
            obj.push(model.name)
            GridView.view.addedDelegates = obj
        }
        GridView.onRemove: {
            view.removedDelegateCount += 1
        }
    }
}

