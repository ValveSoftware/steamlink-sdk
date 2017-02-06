import QtQuick 2.0
import "include_pragma_outer.js" as Script

Item {
    property int test1

    Component.onCompleted: {
        test1 = Script.callFunction()
    }
}

