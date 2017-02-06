import QtQuick 2.0

Item {
    width: 200
    height: 200

    Item {
        objectName: "baselineAnchored"

        width: 200
        height: 10

        anchors.baseline: parent.verticalCenter
    }
}
