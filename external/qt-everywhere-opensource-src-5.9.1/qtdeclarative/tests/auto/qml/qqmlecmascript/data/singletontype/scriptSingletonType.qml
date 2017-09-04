import QtQuick 2.0
import Qt.test.scriptApi 1.0 as QtTestScriptApi                 // script singleton Type installed into new uri

QtObject {
    property int scriptTest: QtTestScriptApi.Script.scriptTestProperty // script singleton types only provide properties.
}
