import QtQuick 2.0
import Qt.test.scriptApi 1.0 as QtTestScriptApi
import Qt.test.scriptApi 2.0 as QtTestScriptApi2

QtObject {
    property int firstProperty
    property int readBack

    property int secondProperty
    property int unchanged

    onFirstPropertyChanged: {
        if (QtTestScriptApi.Script.scriptTestProperty != firstProperty) {
            QtTestScriptApi.Script.scriptTestProperty = firstProperty;
            readBack = QtTestScriptApi.Script.scriptTestProperty;
        }
    }

    onSecondPropertyChanged: {
        if (QtTestScriptApi2.Script.scriptTestProperty != secondProperty) {
            QtTestScriptApi2.Script.scriptTestProperty = secondProperty;
            unchanged = QtTestScriptApi2.Script.scriptTestProperty;
        }
    }

    Component.onCompleted: {
        firstProperty = QtTestScriptApi.Script.scriptTestProperty;
        readBack = QtTestScriptApi.Script.scriptTestProperty;
        secondProperty = QtTestScriptApi2.Script.scriptTestProperty;
        unchanged = QtTestScriptApi2.Script.scriptTestProperty;
    }
}
