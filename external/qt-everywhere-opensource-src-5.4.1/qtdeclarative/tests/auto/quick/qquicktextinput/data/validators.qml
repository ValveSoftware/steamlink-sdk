import QtQuick 2.0

Item {
    property variant intInput: intInput
    property variant dblInput: dblInput
    property variant strInput: strInput
    property variant unvalidatedInput: unvalidatedInput

    width: 800; height: 600;

    Column{
        TextInput { id: intInput;
            property bool acceptable: acceptableInput
            validator: IntValidator{top: 11; bottom: 2}
        }
        TextInput { id: dblInput;
            property bool acceptable: acceptableInput
            validator: DoubleValidator{top: 12.12; bottom: 2.93; decimals: 2; notation: DoubleValidator.StandardNotation}
        }
        TextInput { id: strInput;
            property bool acceptable: acceptableInput
            validator: RegExpValidator { regExp: /[a-zA-z]{2,4}/ }
        }
        TextInput { id: unvalidatedInput
            property bool acceptable: acceptableInput
        }
    }

}
