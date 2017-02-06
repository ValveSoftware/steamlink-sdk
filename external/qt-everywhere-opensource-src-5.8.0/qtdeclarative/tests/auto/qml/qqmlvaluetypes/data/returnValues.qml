import Test 1.0
import QtQuick 2.0

MyTypeObject {
    property bool test1: false;
    property bool test2: false;

    Component.onCompleted: {
        var a = method();

        test1 = (a.width == 13)
        test2 = (a.height == 14)

        size = a;
    }
}

