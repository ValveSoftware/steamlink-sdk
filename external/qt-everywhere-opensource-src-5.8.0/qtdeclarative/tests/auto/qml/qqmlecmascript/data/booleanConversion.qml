import QtQuick 2.0

QtObject {
    id: root

    property bool test_true1: false
    property bool test_true2: false
    property bool test_true3: false
    property bool test_true4: false
    property bool test_true5: false

    property bool test_false1: true
    property bool test_false2: true
    property bool test_false3: true


    Component.onCompleted: {
        test_true1 = 11
        test_true2 = "Hello"
        test_true3 = root
        test_true4 = { a: 10, b: 11 }
        test_true5 = true

        test_false1 = 0
        test_false2 = null
        test_false3 = false
    }
}
