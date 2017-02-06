import Qt.test 1.0


MyQmlObject {
    onSignalWithGlobalName: setString('pass ' + parseInt("5"))
}
