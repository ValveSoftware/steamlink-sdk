import QtQuick 2.0

Column {
    Repeater {
        id: repeater
        objectName: "repeater"

        property int itemsCount
        property variant itemsFound: []

        delegate: Rectangle {
            color: "red"
            width: (index+1)*50
            height: 50
        }

        onModelChanged: {
            repeater.itemsCount = repeater.count
            var items = []
            for (var i=0; i<repeater.count; i++)
                items.push(repeater.itemAt(i))
            repeater.itemsFound = items
        }
    }
}

