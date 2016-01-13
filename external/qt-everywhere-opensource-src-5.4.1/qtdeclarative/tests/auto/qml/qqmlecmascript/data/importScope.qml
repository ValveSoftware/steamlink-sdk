import QtQuick 2.0
import "importScope.1.js" as ImportScope1
import "importScope.2.js" as ImportScope2

QtObject {
    property int test: ImportScope2.getValue()
}
