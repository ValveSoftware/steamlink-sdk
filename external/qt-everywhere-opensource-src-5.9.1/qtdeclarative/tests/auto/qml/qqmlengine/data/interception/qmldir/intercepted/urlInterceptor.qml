import QtQml 2.0
import "intercepted.js" as Script

QtObject {
    property url filePath: "FailsTest"
    property url resolvedUrl: Qt.resolvedUrl("FailsTest");
    property url absoluteUrl: Qt.resolvedUrl("file:///FailsTest");
    property string childString: child.myStr
    property string scriptString: Script.myStr
    property Intercepted child: Intercepted {}
}
