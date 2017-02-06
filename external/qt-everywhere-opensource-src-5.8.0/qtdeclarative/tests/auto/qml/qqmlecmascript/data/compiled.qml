import QtQuick 2.0

QtObject {
    //real
    property real test1: a + b
    property real test2: a - b
    property bool test3: (a < b)
    property bool test4: (a > b)
    property bool test5: (a == b)
    property bool test6: (a != b)

    //int
    property int test7: c + d
    property int test8: d - c
    property bool test9: (c < d)
    property bool test10: (c > d)
    property bool test11: (c == d)
    property bool test12: (c != d)

    //string
    property string test13: e + f
    property string test14: e + " " + f
    property bool test15: (e == f)
    property bool test16: (e != f)

    //type conversion
    property int test17: a
    property real test18: d
    property int test19: g
    property real test20: g
    property string test21: g
    property string test22: h
    property bool test23: i
    property color test24: j
    property color test25: k

    property real a: 4.5
    property real b: 11.2
    property int c: 9
    property int d: 176
    property string e: "Hello"
    property string f: "World"
    property variant g: 6.5
    property variant h: "!"
    property variant i: true
    property string j: "#112233"
    property string k: "#aa112233"
}
