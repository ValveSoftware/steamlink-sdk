import QtQuick 2.0

ListView {
    width: 100
    height: 100

    model: myModel
    delegate: Item {
        objectName: "delegate"
        width: 100
        height: 20
    }
}
