import QtQuick 2.6

Item {
    width: 640
    height: 480
    property bool testRightToLeft: false
    Row {
        objectName: "row"
        layoutDirection: testRightToLeft ? Qt.RightToLeft : Qt.LeftToRight
        padding: 1; topPadding: 1; leftPadding: 1; rightPadding: 1; bottomPadding: 1
        Rectangle {
            objectName: "one"
            color: "red"
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "two"
            color: "red"
            width: 20
            height: 10
        }
        Rectangle {
            objectName: "three"
            color: "red"
            width: 40
            height: 20
        }
    }
}
