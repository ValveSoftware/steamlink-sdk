import QtQuick 2.0
import "include_callback.js" as IncludeTest

QtObject {
    property bool test1: false
    property bool test2: false
    property bool test3: false
    property bool test4: false
    property bool test5: false
    property bool test6: false

    Component.onCompleted: {
        IncludeTest.go();
    }
}
