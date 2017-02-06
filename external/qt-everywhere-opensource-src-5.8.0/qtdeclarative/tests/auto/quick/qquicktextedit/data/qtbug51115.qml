import QtQuick 2.0

TextEdit {
    Component.onCompleted: {
        readOnly = false;
        text= "bla bla";
        selectAll();
        readOnly = true;
    }
}

