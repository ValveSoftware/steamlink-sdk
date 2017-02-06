import QtQuick 2.0

TextInput {
    id: textinput
    property variant regexvalue
    height: 50
    width: 200
    text: "abc"
    validator: RegExpValidator {
        id: regexpvalidator
        regExp: regexvalue
    }
}
