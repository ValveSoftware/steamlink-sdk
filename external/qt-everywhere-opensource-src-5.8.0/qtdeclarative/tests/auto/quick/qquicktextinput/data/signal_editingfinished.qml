import QtQuick 2.2

Item {
    property QtObject input1: input1
    property QtObject input2: input2

    width: 800; height: 600;

    Column{
        TextInput { id: input1;
            property bool acceptable: acceptableInput
            validator: RegExpValidator { regExp: /[a-zA-z]{2,4}/ }
        }
        TextInput { id: input2;
            property bool acceptable: acceptableInput
            validator: RegExpValidator { regExp: /[a-zA-z]{2,4}/ }
        }
    }
}
