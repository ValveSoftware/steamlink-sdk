import QtQuick 2.1

Item {
    width: 400
    height: 600
    function takeOne()
    {
        gridView.model.remove(2)
    }
    function takeThree()
    {
        gridView.model.remove(4)
        gridView.model.remove(2)
        gridView.model.remove(0)
    }
    function takeAll()
    {
        gridView.model.clear()
    }

    GridView {
        id: gridView

        property bool useDelayRemove

        height: parent.height
        width: 400
        cellWidth: width/2
        model: ListModel {
            ListElement { name: "A" }
            ListElement { name: "B" }
            ListElement { name: "C" }
            ListElement { name: "D" }
            ListElement { name: "E" }
        }
        delegate: Text {
            id: wrapper
            height: 100
            text: index + gridView.count
            GridView.delayRemove: gridView.useDelayRemove
            GridView.onRemove: SequentialAnimation {
                PauseAnimation { duration: wrapper.GridView.delayRemove ? 100 : 0 }
                PropertyAction { target: wrapper; property: "GridView.delayRemove"; value: false }
            }
        }
    }
}
