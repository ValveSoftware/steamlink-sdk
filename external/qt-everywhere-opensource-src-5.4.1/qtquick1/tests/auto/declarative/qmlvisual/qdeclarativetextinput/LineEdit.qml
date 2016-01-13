import QtQuick 1.0
import "../shared" 1.0

Item {
    id:lineedit
    property alias text: textInp.text

    width: textInp.width + 11
    height: 13 + 11

    Rectangle {
        color: 'lightsteelblue'
        anchors.fill: parent
    }
    clip: true
    Component.onCompleted: textInp.cursorPosition = 0;
    TestTextInput {
        id:textInp
        cursorDelegate: Item {
            Rectangle {
                visible: parent.parent.focus
                color: "#009BCE"
                height: 13
                width: 2
                y: 1
            }
        }
        property int leftMargin: 6
        property int rightMargin: 6
        x: leftMargin
        y: 5
        //Below function implements all scrolling logic
        onCursorPositionChanged: {
            if(cursorRectangle.x < leftMargin - textInp.x){//Cursor went off the front
                textInp.x = leftMargin - Math.max(0, cursorRectangle.x);
            }else if(cursorRectangle.x > parent.width - leftMargin - rightMargin - textInp.x){//Cusor went off the end
                textInp.x = leftMargin - Math.max(0, cursorRectangle.x - (parent.width - leftMargin - rightMargin));
            }
        }

        autoScroll: false //It is preferable to implement your own scrolling
        text:""
        horizontalAlignment: TextInput.AlignLeft
        font.pixelSize:15
        selectionColor: 'steelblue'
    }
    MouseArea {
        //Implements all line edit mouse handling
        id: mainMouseArea
        anchors.fill: parent;
        function translateX(x){
            return x - textInp.x
        }
        onPressed: {
            textInp.focus = true;
            textInp.cursorPosition = textInp.positionAt(translateX(mouse.x));
        }
        onPositionChanged: {
            textInp.moveCursorSelection(textInp.positionAt(translateX(mouse.x)));
        }
        onReleased: {
        }
        onDoubleClicked: {
            textInp.selectAll()
        }
        z: textInp.z + 1
    }

}
