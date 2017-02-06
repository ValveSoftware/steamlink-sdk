import QtQuick 2.0
import "typeOf.js" as TypeOf

QtObject {
    property string test1
    property string test2
    property string test3
    property string test4
    property string test5
    property string test6
    property string test7
    property string test8
    property string test9

    Component.onCompleted: {
        test1 = TypeOf.test1
        test2 = TypeOf.test2
        test3 = TypeOf.test3
        test4 = TypeOf.test4
        test5 = TypeOf.test5
        test6 = TypeOf.test6
        test7 = TypeOf.test7
        test8 = TypeOf.test8
        test9 = TypeOf.test9
    }
}
