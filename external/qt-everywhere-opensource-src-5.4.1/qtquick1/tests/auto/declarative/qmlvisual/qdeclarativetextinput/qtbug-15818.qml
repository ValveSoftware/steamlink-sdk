import QtQuick 1.1
//Test that doubleclicking on the front of a word only selects that word, and not the word in front

Item{
    width: 200
    height: 100
    TextInput{
        anchors.fill: parent
        readOnly: true
        selectByMouse: true
        text: "abc a cba test"
    }
}
