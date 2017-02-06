import Qt.test 1.0
import QtQuick 2.0

QtObject {
    property real test: 1234567890
    property real test2
    property real test3
    property real test4: test3
    property real test5: func()
    property real test6: test2 + test3

    signal sig(real arg)

    Component.onCompleted: {
        test2 = 1234567890;
        sig(1234567890)
    }

    onSig: { test3 = arg; }

    function func() { return 1234567890; }
}
