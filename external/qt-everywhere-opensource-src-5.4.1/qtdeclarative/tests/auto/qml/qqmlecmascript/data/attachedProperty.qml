import Qt.test 1.0
import Qt.test 1.0 as Namespace

MyQmlObject {
    id: me
    property int a: MyQmlObject.value
    property int b: Namespace.MyQmlObject.value
    property int c: me.Namespace.MyQmlObject.value
    property int d: me.Namespace.MyQmlObject.value
}

