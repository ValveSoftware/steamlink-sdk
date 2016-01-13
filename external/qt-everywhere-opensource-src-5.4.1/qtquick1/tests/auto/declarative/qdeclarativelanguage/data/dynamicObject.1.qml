import Test 1.0
import QtQuick 1.0
MyCustomParserType {
    propa: a + 10
    propb: Math.min(a, 10)
    propc: MyPropertyValueSource {}
    onPropA: a
}
