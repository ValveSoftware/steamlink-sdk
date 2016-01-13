import QtQuick 1.0

Item {
    id: root

    property int a: 1
    property int binding: a
    property string binding2: a + "Test"
    property int binding3: myFunction()
    property int binding4: nestedObject.myNestedFunction()

    function myFunction() {
        return a;
    }

    Item {
        id: nestedObject

        function myNestedFunction() {
            return a;
        }

        property int a: 2
        property int binding: a
        property string binding2: a + "Test"
        property int binding3: myFunction()
        property int binding4: myNestedFunction()
    }

    ScopeObject {
        id: compObject
    }

    property alias test1: root.binding
    property alias test2: nestedObject.binding
    property alias test3: root.binding2
    property alias test4: nestedObject.binding2
    property alias test5: root.binding3
    property alias test6: nestedObject.binding3
    property alias test7: root.binding4
    property alias test8: nestedObject.binding4
    property alias test9: compObject.binding
    property alias test10: compObject.binding2
}
