import QtQuick 2.0

Grid {
    Repeater {
        width: 100
        height: 100

        model: myModel
        delegate: Item {
            objectName: "delegate"
            width: 50
            height: 50
        }
    }
}
