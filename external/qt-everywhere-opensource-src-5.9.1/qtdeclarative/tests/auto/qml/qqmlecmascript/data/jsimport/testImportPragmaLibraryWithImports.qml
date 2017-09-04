import QtQuick 2.0
import "importPragmaLibraryWithImports.js" as LibraryImport

QtObject {
    id: root
    property int testValue: LibraryImport.importIncrementedValue(); // valueOf(4 + 1 + '5') = valueOf('55') = 55
}
