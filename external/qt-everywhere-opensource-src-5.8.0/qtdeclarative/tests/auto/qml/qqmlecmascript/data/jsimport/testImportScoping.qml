import QtQuick 2.0

// For backward compatibility, importing a script which has no imports,
// should run the script in the parent context.  See QTBUG-17518.

import "importWithNoImports.js" as TestNoImportScoping

QtObject {
    id: testQtObject
    property int componentError: TestNoImportScoping.componentError()
}
