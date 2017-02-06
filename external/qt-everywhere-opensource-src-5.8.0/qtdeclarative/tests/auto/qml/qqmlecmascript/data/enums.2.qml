import Qt.test 1.0
import Qt.test 1.0 as Namespace

MyQmlObject {
    id: root

    property int a: MyQmlObject.EnumValue10
    property int b: Namespace.MyQmlObject.EnumValue10
    property int c: child.enumProperty

    // test binding codepath
    property MyUnregisteredEnumTypeObject child: MyUnregisteredEnumTypeObject {
        enumProperty: undefined
    }

    // test assignment codepaths
    function testAssignmentOne() {
        child.enumProperty = function() { return "Hello, world!" };   // cannot assign function to non-var prop.
    }
    function testAssignmentTwo() {
        child.enumProperty = MyUnregisteredEnumTypeObject.Firstvalue; // note: incorrect capitalisation
    }
    function testAssignmentThree() {
        child.enumProperty = undefined;                               // directly set undefined value
    }



    property int d: 5
    property MyUnregisteredEnumTypeObject child2: MyUnregisteredEnumTypeObject {
        enumProperty: root.d
    }
    function testAssignmentFour() {
        child2.enumProperty = root.d;
    }
}

