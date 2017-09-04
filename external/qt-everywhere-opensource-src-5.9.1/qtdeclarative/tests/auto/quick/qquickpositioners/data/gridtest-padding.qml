import QtQuick 2.6

Item {
    width: 640
    height: 480
    property bool testRightToLeft: false
    property int testHAlignment: Grid.AlignLeft;
    property int testVAlignment: Grid.AlignTop;
    Grid {
        layoutDirection: testRightToLeft ? Qt.RightToLeft : Qt.LeftToRight
        horizontalItemAlignment: testHAlignment
        verticalItemAlignment: testVAlignment
        objectName: "grid"
        columns: 3
        padding: 1; topPadding: 1; leftPadding: 1; rightPadding: 1; bottomPadding: 1
        Rectangle {
            objectName: "one"
            color: "red"
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "two"
            color: "green"
            width: 20
            height: 50
        }
        Rectangle {
            objectName: "three"
            color: "blue"
            width: 30
            height: 20
        }
        Rectangle {
            objectName: "four"
            color: "cyan"
            width: 50
            height: 50
        }
        Rectangle {
            objectName: "five"
            color: "magenta"
            width: 10
            height: 10
        }
    }
}
