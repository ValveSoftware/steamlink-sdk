import QtQuick 2.0

QtObject {
    property bool runTest: false
    onRunTestChanged: test1 = undefined

    property variant test1: 10
    property variant test2: (runTest == false)?11:undefined
}
