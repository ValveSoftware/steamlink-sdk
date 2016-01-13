import QtQuick 2.2

Item {
    property QtObject input: input

    width: 800; height: 600;

    Column{
        TextInput { id: input;
            property bool acceptable: acceptableInput
            validator: RegExpValidator { regExp: /[a-zA-z]{2,4}/ }
        }
    }
}
