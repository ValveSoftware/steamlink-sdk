import QtQuick 1.1
import "../../shared" 1.0

Rectangle {
    width: childrenRect.width
    height: childrenRect.height

    TestText {
        width:  80
        maximumLineCount: 2
        text: "Line1\nLine2\nLine3\nLine4"
    }
}
