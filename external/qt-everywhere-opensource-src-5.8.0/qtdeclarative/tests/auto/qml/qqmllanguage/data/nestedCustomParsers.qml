import Test 1.0
import QtQml 2.0
SimpleObjectWithCustomParser {
    customProperty: 42
    property var nested: SimpleObjectWithCustomParser {
        customNestedProperty: 42
    }
}
