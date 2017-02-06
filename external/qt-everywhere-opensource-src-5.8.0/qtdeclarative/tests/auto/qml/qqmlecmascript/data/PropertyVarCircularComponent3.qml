import QtQuick 2.0

Rectangle {
    id: rectangle      // will have JS ownership
    objectName: "rectangle"
    width: 10
    height: 10
    property var rectCanary: 5

    Text {
        id: text // will have Eventual-JS ownership
        objectName: "text"
        property var vp: rectangle
        property var textCanary: 10
    }
}
