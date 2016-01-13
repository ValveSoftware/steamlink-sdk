import QtQuick 1.0
import "libraryScriptAssert.js" as Test

QtObject {
    id: root
    Component.onCompleted: Test.test(root);
}
