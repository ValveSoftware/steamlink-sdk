import QtQuick 1.0

ListView {
    id: view
    width: 240; height: 320

    property variant addedDelegates: []
    property int removedDelegateCount

    model: testModel

    delegate: Rectangle {
        width: 200; height: delegateHeight
        border.width: 1
        ListView.onAdd: {
            var obj = ListView.view.addedDelegates
            obj.push(model.name)
            ListView.view.addedDelegates = obj
        }
        ListView.onRemove: {
            view.removedDelegateCount += 1
        }
    }
}
