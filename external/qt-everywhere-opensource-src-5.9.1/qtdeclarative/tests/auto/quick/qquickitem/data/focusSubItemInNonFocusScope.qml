import QtQuick 2.0

Rectangle {
    width: 400; height: 400

    FocusScope {
        width: 400; height: 400
        focus: true
        Item {
            width: 400; height: 400
            Item {
                id: dummy
                objectName: "dummyItem"
                focus: true
            }
            TextInput {
                id: ti
                objectName: "textInput"
                focus: true
            }
        }
    }
}
