import Qt.test 1.0
import QtQuick 2.0

MyQmlObject {
    function doIt() {
        d.hello("World")
    }

    property QtObject subobject: QtObject {
        id: d
        function hello(input) {
            var temp = "Hello " + input;
            var input = temp + "!";
            prop = input
        }
    }

    property string prop
}
