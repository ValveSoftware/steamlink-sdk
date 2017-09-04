import Qt.test 1.0
import Qt.test 1.0 as Namespace

MyQmlObject {
    property alias a: me.a
    property alias b: me.a
    property alias c: me.a
    property alias d: me.a

    property MyQmlObject obj
    obj: MyQmlObject {
        MyQmlObject.value2: 13

        id: me
        property int a: MyQmlObject.value2 * 2
        property int b: Namespace.MyQmlObject.value2 * 2
        property int c: me.Namespace.MyQmlObject.value * 2
        property int d: me.Namespace.MyQmlObject.value * 2
    }
}


