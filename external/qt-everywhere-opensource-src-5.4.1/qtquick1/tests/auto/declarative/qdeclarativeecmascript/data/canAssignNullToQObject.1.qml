import Qt.test 1.0

MyQmlObject {
    property bool runTest: false

    property variant a: MyQmlObject {}

    objectProperty: (runTest == false)?a:null
}
