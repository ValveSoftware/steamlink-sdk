import QtQuick 2.0

Item {
    id: root;
    property bool test: str == str2 && (txt != null && txt.str == root.str)
    property Text txt: null
    //Constant doesn't hit rewriter
    property string str: 'same
multiline
string 5 !'
    property string str2: '';
    Component {
        id: comp
        Text {
            property var value: 1
            property string str: 'same
multiline
string ' + value + " !"
            Component.onCompleted: { //Separate codepath for signal handers in rewriter
                root.str2 = 'same
multiline
string ' + value + " !"
            }
        }
    }
    Component.onCompleted: txt = comp.createObject(root,{"value" : 5})
    /*
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: console.debug( "Test: " + test + '\n' + str + '\n:\n' + str2 + "\n:\n" + txt.str)
    }
    */
}
