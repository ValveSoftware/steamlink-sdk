import Qt.test 1.0

MyQmlObject {
    property alias c1: myConstants.c1
    property alias c3: myConstants.c3

    objectProperty: ConstantsOverrideBindings {
        id: myConstants
        c3: 10
    }
}
