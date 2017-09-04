import Test 1.0
import QtQuick 2.0
import "deletedObject.js" as JS

MyTypeObject {
    property variant object

    object: MyTypeObject {}
    Component.onCompleted: JS.startup()
    onRunScript: JS.afterDelete()
}
