import QtQml 2.0
import "."
import "intercepted.js" as Script

QtObject {
    property url filePath: "doesNotExist.file"
    property url resolvedUrl: Qt.resolvedUrl("doesNotExist.file");
    property url absoluteUrl: Qt.resolvedUrl("file:///doesNotExist.file");
    property string childString: child.myStr
    property string scriptString: Script.myStr
    property Intercepted child: Intercepted {}
}
