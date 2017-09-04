import QtQuick 2.0

Item {
    width: 640
    height: 480
    property bool testRightToLeft: false
    property bool testEnabled: false

    Row {
        objectName: "row"
        layoutDirection: testRightToLeft ? Qt.RightToLeft : Qt.LeftToRight
        populate: Transition {
            enabled: testEnabled ? false : true
            NumberAnimation {
                properties: "x";
            }
        }
        add: Transition {
            enabled: testEnabled ? false : true
            NumberAnimation {
                properties: "x";
            }
        }
        move: Transition {
            enabled: testEnabled ? false : true
            NumberAnimation {
                properties: "x";
            }
        }
        Rectangle {
            objectName: "one"
            color: "red"
            x: -100;
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "two"
            color: "blue"
            x: -100;
            visible: false
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "three"
            x: -100;
            color: "green"
            width: 50
            height: 50
        }
    }
}
