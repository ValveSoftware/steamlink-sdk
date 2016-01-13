import QtQuick 2.0

GridView {
    width: 400
    height: 200
    cellWidth: width/9
    cellHeight: height/2

    model: testModel
    delegate: Rectangle {
        objectName: "wrapper"; width: 10; height: 10; color: "green"
        Text { text: index }
    }
}

