import QtQml 2.2
import x.y.z 1.0

QtObject {
    function x() {
        eval("1");
        return ExternalEnums.DocumentsLocation;
    }

    property var a: ExternalEnums.DocumentsLocation
    property var b: x()
}
