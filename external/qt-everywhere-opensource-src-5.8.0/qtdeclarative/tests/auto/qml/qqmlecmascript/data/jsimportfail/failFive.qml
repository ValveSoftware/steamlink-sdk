import QtQuick 2.0

// This should fail, since if the script does have imports
// of its own, it should run in its own context.

import "importWithImports.js" as TestImportScoping

QtObject {
    id: testQtObject
    property int componentError: TestImportScoping.componentError()
}
