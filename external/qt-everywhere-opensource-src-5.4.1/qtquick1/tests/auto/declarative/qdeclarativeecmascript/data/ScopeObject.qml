import QtQuick 1.0

Item {
    property int a: 3
    property int binding: myFunction();
    property int binding2: myCompFunction();

    function myCompFunction() {
        return a;
    }
}

