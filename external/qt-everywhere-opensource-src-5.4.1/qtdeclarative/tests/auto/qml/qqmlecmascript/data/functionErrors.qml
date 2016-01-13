import QtQuick 2.0

QtObject {
    function myFunction() {
        a = 10;
    }

    Component.onCompleted: myFunction();
}

