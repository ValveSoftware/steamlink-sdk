import QtQuick 1.0

Rectangle {
    id: root
    width:300; height:400

    property bool focus1: root.activeFocus
    property bool focus2: item1.activeFocus
    property bool focus3: fs1.activeFocus
    property bool focus4: fs2.activeFocus
    property bool focus5: theItem.activeFocus

    Item {
        id: item1
        FocusScope {
            id: fs1
            focus: true
            FocusScope {
                id: fs2
                focus: true
                Item {
                    id: theItem
                    focus: true
                }
            }
        }
    }
}
