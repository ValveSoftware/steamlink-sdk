import QtQuick 1.0

QtObject {
    property bool test1: false;
    property bool test2: false;
    property bool test3: false;
    property bool test4: false;
    property bool test5: false;


    property int a: 7
    property int b: 8

    Component.onCompleted: {
        var b = 9;

        test1 = (eval("a") == 7);
        test2 = (eval("b") == 9);
        try {
            eval("c");
        } catch(e) {
            test3 = true;
        }
        test4 = (eval("console") == console);
        test5 = (eval("Qt") == Qt);
    }
}
