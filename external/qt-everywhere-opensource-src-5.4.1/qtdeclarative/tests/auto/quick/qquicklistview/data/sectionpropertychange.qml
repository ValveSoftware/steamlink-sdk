import QtQuick 2.0

Rectangle {
    width: 320; height: 480

    Rectangle {
        id: groupButtons
        width: 300; height: 30
        color: "yellow"
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: "swap"
        }
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
        }
        MouseArea {
            anchors.fill: parent
            onClicked: switchGroups()
        }
    }

    function switchGroups() {
        myListView.model.move(0,1,1)
        if ("title" === myListView.groupBy)
            myListView.groupBy = "genre"
        else
            myListView.groupBy = "title"
    }

    function switchGrouped() {
        if ("pageCount" === myListView.groupBy)
            myListView.groupBy = "genre"
        else
            myListView.groupBy = "pageCount"
    }

    Component.onCompleted: {
        myListView.model = generateModel(myListView)
    }

    ListView {
        id: myListView
        objectName: "list"

        clip: true
        property string groupBy: "title"

        anchors {
            top: groupButtons.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        delegate: Item {
            objectName: "wrapper"
            height: 50
            width: 320
            Text { id: t; text: model.title }
            Text { text: model.author; font.pixelSize: 10; anchors.top: t.bottom }
            Text { text: parent.y; anchors.right: parent.right }
        }

        section {
            criteria: ViewSection.FullString
            delegate: Rectangle { width: 320; height: 25; color: "lightblue"
                objectName: "sect"
                Text {text: section }
                Text { text: parent.y; anchors.right: parent.right }
            }
            property: myListView.groupBy
        }
    }

    function generateModel(theParent)
    {
        var books = [
                { "author": "Billy Bob", "genre": "Anarchism", "title": "Frogs and Love", "pageCount": 80 },
                { "author": "Lefty Smith", "genre": "Horror", "title": "Chainsaws for Noobs", "pageCount": 80 }
                ];

        var model = Qt.createQmlObject("import QtQuick 2.0; ListModel {}", theParent);

        for (var i = 0; i < books.length; ++i) {
            var book = books[i];
            model.append(book);
        }
        return model;
    }

}

