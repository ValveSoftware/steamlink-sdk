import QtQuick 2.0

ListView {
    width: 320; height: 480
    model: ListModel {}
    cacheBuffer: 300
    delegate: Rectangle {
        objectName: "wrapper"
        width: parent.width
        height: parent.parent.height/12
        color: index % 2 ? "red" : "blue"
    }
    Component.onCompleted: {
        for (var i = 0; i < 100; ++i)
            model.append({"foo":"bar"+i})
    }
}
