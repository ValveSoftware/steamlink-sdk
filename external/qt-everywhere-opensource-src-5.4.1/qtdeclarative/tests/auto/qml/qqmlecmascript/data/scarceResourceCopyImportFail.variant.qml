import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceCopyImportFail.variant.js" as ScarceResourceCopyImportFailJs

QtObject {
    property variant scarceResourceCopy: ScarceResourceCopyImportFailJs.importScarceResource()
}

