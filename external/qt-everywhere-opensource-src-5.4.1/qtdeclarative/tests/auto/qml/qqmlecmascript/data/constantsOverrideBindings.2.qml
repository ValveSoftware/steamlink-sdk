import Qt.test 1.0

MyQmlObject {
    property alias c1: myConstants.c1
    property alias c2: myConstants.c2

    objectProperty: ConstantsOverrideBindings {
        id: myConstants
        c2: 10
    }
}
