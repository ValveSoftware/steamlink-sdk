import QtQuick 2.0

Column {
    FocusScope {
        objectName: "scope1"
        width: 20 ;height: 20
        focus: true
        Rectangle {
            objectName: "item1"
            anchors.fill: parent
            focus: true
        }
    }
    FocusScope {
        objectName: "scope2"
        width: 20 ;height: 20
        Rectangle {
            objectName: "item2"
            anchors.fill: parent
        }
    }
}
