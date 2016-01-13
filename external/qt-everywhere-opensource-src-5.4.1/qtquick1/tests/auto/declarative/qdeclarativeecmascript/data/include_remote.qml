import QtQuick 1.0
import "include_remote.js" as IncludeTest

QtObject {
    property bool done: false
    property bool done2: false

    property bool test1: false
    property bool test2: false
    property bool test3: false
    property bool test4: false
    property bool test5: false

    property bool test6: false
    property bool test7: false
    property bool test8: false
    property bool test9: false
    property bool test10: false

    Component.onCompleted: IncludeTest.go();
}
