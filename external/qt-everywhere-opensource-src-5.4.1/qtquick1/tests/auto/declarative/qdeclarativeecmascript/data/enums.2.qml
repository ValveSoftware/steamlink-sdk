import Qt.test 1.0
import Qt.test 1.0 as Namespace

MyQmlObject {
    property int a: MyQmlObject.EnumValue10
    property int b: Namespace.MyQmlObject.EnumValue10
}

