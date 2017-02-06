import QtQuick 2.0

TextInput {
    Component.onCompleted: {
        readOnly = false;
        text= "bla bla";
        selectAll();
        readOnly = true;
    }
}

