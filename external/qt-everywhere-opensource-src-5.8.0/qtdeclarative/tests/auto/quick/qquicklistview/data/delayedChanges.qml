import QtQuick 2.1

Item {
    width: 400
    height: 400
    function takeTwo()
    {
        listView.model.remove(0);
        listView.model.remove(0);
    }
    function takeTwo_sync()
    {
        listView.model.remove(0);
        listView.forceLayout();
        listView.model.remove(0);
        listView.forceLayout();
    }

    ListView {
        id: listView
        height: parent.height
        width: 400
        model: ListModel {
            ListElement { name: "A" }
            ListElement { name: "B" }
            ListElement { name: "C" }
            ListElement { name: "D" }
            ListElement { name: "E" }
            ListElement { name: "F" }
            ListElement { name: "G" }
            ListElement { name: "H" }
            ListElement { name: "I" }
            ListElement { name: "J" }
        }
        delegate: Text {
            text: index + listView.count
        }
    }
}
