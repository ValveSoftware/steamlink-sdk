import QtQuick 2.0

QtObject {
    property bool test1: false;
    property bool test2: false;
    property bool test3: false;

    Component.onCompleted: {
        var a = 10;

        var func1 = new Function("a", "return a + 7");
        var func2 = new Function("a", "return Qt.atob(a)");
        var func3 = new Function("return a");

        test1 = (func1(4) == 11);
        test2 = (func2("Hello World!") == Qt.atob("Hello World!"));
        try {
            func3();
        } catch(e) {
            test3 = true;
        }
    }
}
