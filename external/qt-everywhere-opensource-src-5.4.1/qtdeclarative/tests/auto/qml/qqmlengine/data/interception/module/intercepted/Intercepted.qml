import QtQml 2.0

QtObject {
    property url filePath: "doesNotExist.file"
    property url resolvedUrl: Qt.resolvedUrl("doesNotExist.file");
    property url absoluteUrl: Qt.resolvedUrl("file:///doesNotExist.file");
    property string childString: "intercepted"
    property string scriptString: "intercepted"
}
