import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool result
    stringProperty: "http://example.org"
    urlProperty: stringProperty + "/?get%3cDATA%3e"
    value: urlProperty == stringProperty + "/?get%3cDATA%3e"
    result: urlProperty == urlProperty
}
