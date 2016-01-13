import QtQuick 1.1
import "../../shared" 1.0

Rectangle {
    width: childrenRect.width
    height: childrenRect.height

    TestText {
        width:  80
        maximumLineCount: 3
        wrapMode: Text.WordWrap
        text: "The quick brown fox jumped over the lazy dog"
    }
}
