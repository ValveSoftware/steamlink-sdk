import QtQuick 2.0
import Qt.test.scriptApi 1.0 as QtTestScriptApi                   // script singleton Type installed into new uri

QtObject {
    property int scriptTest: QtTestScriptApi.Script.scriptTestProperty

    function modifyValues() {
        // the constructor function of the script singleton will modify
        // the value if it were called again (via the static int increment).
        // So, we don't need to do anything in this function.
    }
}
