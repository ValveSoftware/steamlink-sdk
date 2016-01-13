import QtQuick 1.0

Rectangle {
    width: 200
    height: 200

    FocusScope {
        focus: true
        Rectangle {
            objectName: "item1"
            color: "blue"
            onFocusChanged: focus ? color = "red" : color = "blue"
        }
        Rectangle {
            objectName: "item2"
            color: "blue"
            onFocusChanged: focus ? color = "red" : color = "blue"
        }
    }

    FocusScope {
        Rectangle {
            objectName: "item3"
            color: "blue"
            onFocusChanged: focus ? color = "red" : color = "blue"
        }
        Rectangle {
            objectName: "item4"
            color: "blue"
            onFocusChanged: focus ? color = "red" : color = "blue"
        }
    }
}
