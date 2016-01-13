import QtQuick 1.0
import "../shared" 1.0

Item {
    id:lineedit
    property alias text: textEdit.text

    width: 240 + 11 //Should be set manually in most cases
    height: textEdit.height + 11

    Rectangle {
        color: 'lightsteelblue'
        anchors.fill: parent
    }
    clip: true
    Component.onCompleted: textEdit.cursorPosition = 0;
    TestTextEdit {
        id:textEdit
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
        property int topMargin: 6
        property int rightMargin: 6
        property int bottomMargin: 6
        x: leftMargin
        width: parent.width - leftMargin - rightMargin;
        y: 5
        //Below function implements all scrolling logic
        onCursorPositionChanged: {
            if(cursorRectangle.y < topMargin - textEdit.y){//Cursor went off the front
                textEdit.y = topMargin - Math.max(0, cursorRectangle.y);
            }else if(cursorRectangle.y > parent.height - topMargin - bottomMargin - textEdit.y){//Cursor went off the end
                textEdit.y = topMargin - Math.max(0, cursorRectangle.y - (parent.height - topMargin - bottomMargin) + cursorRectangle.height);
            }
        }
        onHeightChanged: y=topMargin//reset scroll

        text:""
        horizontalAlignment: TextInput.AlignLeft
        wrapMode: TextEdit.WordWrap
        font.pixelSize:15
        selectionColor: 'steelblue'
    }
    MouseArea {
        //Implements all line edit mouse handling
        id: mainMouseArea
        anchors.fill: parent;
        function translateY(y){
            return y - textEdit.y
        }
        function translateX(x){
            return x - textEdit.x
        }
        onPressed: {
            textEdit.focus = true;
            textEdit.cursorPosition = textEdit.positionAt(translateX(mouse.x), translateY(mouse.y));
        }
        onPositionChanged: {
            textEdit.moveCursorSelection(textEdit.positionAt(translateX(mouse.x), translateY(mouse.y)));
        }
        onReleased: {
        }
        onDoubleClicked: {
            textEdit.selectAll()
        }
        z: textEdit.z + 1
    }

}
