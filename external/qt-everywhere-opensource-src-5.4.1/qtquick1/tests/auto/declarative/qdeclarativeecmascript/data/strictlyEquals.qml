import QtQuick 1.0

QtObject {
    property bool test1: (a === true)
    property bool test2: !(a === false)
    property bool test3: (b === 11.2)
    property bool test4: !(b === 9)
    property bool test5: (c === 9)
    property bool test6: !(c === 13)
    property bool test7: (d === "Hello world")
    property bool test8: !(d === "Hi")

    property bool a: true
    property real b: 11.2
    property int c: 9
    property string d: "Hello world"
}
