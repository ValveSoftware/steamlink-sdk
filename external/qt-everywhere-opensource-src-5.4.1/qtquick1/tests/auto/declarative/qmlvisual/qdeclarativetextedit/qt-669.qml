import QtQuick 1.0
import "../shared" 1.0

Rectangle {
    Component { id: testableCursor
        //This shouldn't blink
        Rectangle {
            color:"black"
            width:1
        }
    }
    width:300;
    height:40;
    TestTextEdit {
        focus: true;
        cursorDelegate: testableCursor
        text: "Jackdaws love my big sphinx of Quartz"
    }
}
