import QtQuick 2.1

Item {
    width: 400
    height: 600
    function takeOne()
    {
        listView.model.remove(2)
    }
    function takeThree()
    {
        listView.model.remove(4)
        listView.model.remove(2)
        listView.model.remove(0)
    }
    function takeAll()
    {
        listView.model.clear()
    }

    ListView {
        id: listView

        property bool useDelayRemove

        height: parent.height
        width: 400
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
            text: index + listView.count
            ListView.delayRemove: listView.useDelayRemove
            ListView.onRemove: SequentialAnimation {
                PauseAnimation { duration: wrapper.ListView.delayRemove ? 100 : 0 }
                PropertyAction { target: wrapper; property: "ListView.delayRemove"; value: false }
            }
        }
    }
}
