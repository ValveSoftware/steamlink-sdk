import QtQuick 2.0

Rectangle {
    width: 240
    height: 320
    color: "#ffffff"

    ListModel { id: testModel }

    ListView {
        id: list
        objectName: "list"
        width: parent.width
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        model: testModel

        delegate: Text {
            objectName: "wrapper"
            font.pointSize: 20
            text: index
        }
        footer: Rectangle {
            width: parent.width
            height: 40
            color: "green"
        }
        header: Text { objectName: "header"; text: "Header" }
    }

    Component.onCompleted: {
        if (setCurrentToZero == 0)
            list.currentIndex = 0
        for (var i=0; i<30; i++) testModel.append({"name" : i, "val": i})
    }
}
