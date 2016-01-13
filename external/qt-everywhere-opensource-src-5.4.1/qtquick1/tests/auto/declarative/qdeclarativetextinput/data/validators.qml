import QtQuick 1.0

Item {
    property variant intInput: intInput
    property variant dblInput: dblInput
    property variant strInput: strInput

    width: 800; height: 600;

    Column{
        TextInput { id: intInput;
            validator: IntValidator{top: 11; bottom: 2}
        }
        TextInput { id: dblInput;
            validator: DoubleValidator{top: 12.12; bottom: 2.93; decimals: 2; notation: DoubleValidator.StandardNotation}
        }
        TextInput { id: strInput;
            validator: RegExpValidator { regExp: /[a-zA-z]{2,4}/ }
        }
    }

}
