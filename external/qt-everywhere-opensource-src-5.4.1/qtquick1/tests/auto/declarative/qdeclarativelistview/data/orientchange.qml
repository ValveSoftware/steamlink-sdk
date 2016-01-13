import QtQuick 1.0

ListView {
    width: 240; height: 320
    delegate: Rectangle { objectName: "wrapper"; width: 80; height: 80 }
    model: 100
}
