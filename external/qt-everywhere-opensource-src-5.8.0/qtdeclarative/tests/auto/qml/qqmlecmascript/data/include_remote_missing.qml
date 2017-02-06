import QtQuick 2.0
import "include_remote_missing.js" as IncludeTest

QtObject {
    property bool done: false

    property bool test1: false
    property bool test2: false
    property bool test3: false

    property string serverBaseUrl;

    Component.onCompleted: IncludeTest.go(serverBaseUrl);
}
