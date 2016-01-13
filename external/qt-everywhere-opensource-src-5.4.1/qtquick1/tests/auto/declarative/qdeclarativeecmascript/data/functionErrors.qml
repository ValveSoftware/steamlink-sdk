import QtQuick 1.0

QtObject {
    function myFunction() {
        a = 10;
    }

    Component.onCompleted: myFunction();
}

