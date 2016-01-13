import QtQuick 1.0
import "../../shared" 1.0

Rectangle {
    width: childrenRect.width
    height: childrenRect.height
    Column {
        width: 80
        height: myText.height*4
        TestText {
            elide: "ElideLeft"
            text: "aaa bbb ccc ddd eee fff"
            width: 80
            id: myText
        }
        TestText {
            elide: "ElideMiddle"
            text: "aaa bbb ccc ddd eee fff"
            width: 80
        }
        TestText {
            elide: "ElideRight"
            text: "aaa bbb ccc ddd eee fff"
            width: 80
        }
        TestText {
            elide: "ElideNone"
            text: "aaa bbb ccc ddd eee fff"
            width: 80
        }
    }
}
