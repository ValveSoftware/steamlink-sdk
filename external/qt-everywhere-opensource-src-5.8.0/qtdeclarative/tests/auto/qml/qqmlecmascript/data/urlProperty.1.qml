import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    property bool result
    urlProperty: stringProperty + "/index.html"
    intProperty: if (urlProperty) 123; else 321
    value: urlProperty == stringProperty + "/index.html"
    result: urlProperty == urlProperty
}
