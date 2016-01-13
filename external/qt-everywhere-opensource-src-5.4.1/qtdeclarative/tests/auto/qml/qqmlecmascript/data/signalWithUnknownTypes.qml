import Qt.test 1.0

MyQmlObject {
    onSignalWithUnknownType: variantMethod(arg);
    onSignalWithCompletelyUnknownType: variantMethod(arg)
}
