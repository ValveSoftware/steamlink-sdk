import QtQuick 2.0

Item {
    property bool test: false
    property string string1
    property string string2

    property var f1 : function () {
        string1 = "Test case 1"
        return 100;
    }

    property var f2;
    function testcase2() {
        string2 = "Test case 2"
        return 100;
    }

    f2: testcase2

    Component.onCompleted: {
        if (f1() != 100)
            return

        if (f2() != 100)
            return;

        if (string1 != "Test case 1")
            return;

        if (string2 != "Test case 2")
            return;

        test = true;
    }
}
