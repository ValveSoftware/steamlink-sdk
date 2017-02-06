import Qt.test 1.0

MyQmlObject {
    property int c1: 0
    property int c2: c1
    property alias c3: inner.ic1

    objectProperty: MyQmlObject {
        id: inner
        property int ic1: c1
    }
}
