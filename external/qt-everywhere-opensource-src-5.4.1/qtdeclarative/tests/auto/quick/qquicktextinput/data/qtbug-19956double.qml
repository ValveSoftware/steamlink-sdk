import QtQuick 2.0

TextInput {
    id: textinput
    property real topvalue: 30
    property real bottomvalue: 10
    height: 50
    width: 200
    text: "20"
    validator: DoubleValidator {
        id: doublevalidator
        bottom: bottomvalue
        top: topvalue
    }
}
