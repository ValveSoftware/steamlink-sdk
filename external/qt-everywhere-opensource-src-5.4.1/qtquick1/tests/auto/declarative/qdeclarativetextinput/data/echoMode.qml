import QtQuick 1.0

Rectangle {
    property QtObject myInput: input

    width: 400; height: 200; color: "green"

    TextInput { id: input; focus: true
        text: "ABCDefgh"
    }
}
