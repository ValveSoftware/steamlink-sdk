import QtQuick 2.0
import Qt.test.scriptApi 1.0

QtObject {
    property int scriptTest: Script.scriptTestProperty // script singleton type's only provide properties.
}
