import Qt.test 1.0

MyQmlObject {
    property variant a: function myFunction() { return 2; }
    property variant b: Qt.binding(function() { return 2; })
    property var c: Qt.binding(function() { return 2; })
    qjsvalue: Qt.binding(function() { return 2; })
}
