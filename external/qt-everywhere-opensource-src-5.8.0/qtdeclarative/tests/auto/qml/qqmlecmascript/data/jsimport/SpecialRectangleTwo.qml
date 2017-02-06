import QtQuick 2.0

import "importPragmaLibrary.js" as TestPragmaLibraryImport

Rectangle {
    width: TestPragmaLibraryImport.importIncrementedValue()
    height: width + 5
    color: "blue"
}
