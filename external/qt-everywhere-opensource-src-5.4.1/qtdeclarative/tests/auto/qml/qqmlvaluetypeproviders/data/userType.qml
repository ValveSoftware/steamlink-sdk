import QtQuick 2.0
import Test 1.0

Item {
    property bool success: false

    // Test user value type stored as both var and variant
    property var testValue1
    property variant testValue2
    property variant testValue3
    property var testValue4

    TestValueExporter {
        id: assignmentValueType
        testValue.property1: 1
        testValue.property2: 3.1415927
    }

    TestValueExporter {
        id: v4BindingValueType
        testValue.property1: 1 + 2
        testValue.property2: 3.1415927 / 2.0
    }

    TestValueExporter {
        id: v8BindingValueType
        testValue.property1: if (true) 1 + 2
        testValue.property2: if (true) 3.1415927 / 2.0
    }

    function numberEqual(lhs, rhs) {
        var d = (lhs - rhs)
        return (Math.abs(d) < 0.0001)
    }

    Component.onCompleted: {
        // Poperties assigned the result of Q_INVOKABLE:
        testValue1 = testValueExporter.getTestValue()
        testValue2 = testValueExporter.getTestValue()

        if (testValue1.property1 != 333) return
        if (!numberEqual(testValue1.property2, 666.999)) return

        if (testValue2.property1 != 333) return
        if (!numberEqual(testValue2.property2, 666.999)) return

        if (testValue1 != testValue2) return

        // Write to the properties of the value type
        testValue1.property1 = 1
        testValue1.property2 = 3.1415927

        testValue2.property1 = 1
        testValue2.property2 = 3.1415927

        if (testValue1.property1 != 1) return
        if (!numberEqual(testValue1.property2, 3.1415927)) return

        if (testValue2.property1 != 1) return
        if (!numberEqual(testValue2.property2, 3.1415927)) return

        if (testValue1 != testValue2) return

        // Assignment of value type properties
        testValue3 = testValue1
        testValue4 = testValue2

        if (testValue3.property1 != 1) return
        if (!numberEqual(testValue3.property2, 3.1415927)) return

        if (testValue4.property1 != 1) return
        if (!numberEqual(testValue4.property2, 3.1415927)) return

        if (testValue3 != testValue4) return

        // Access a value-type property of a QObject
        var vt = testValueExporter.testValue
        if (vt.property1 != 0) return
        if (!numberEqual(vt.property2, 0.0)) return

        testValueExporter.testValue = testValue4

        if (vt.property1 != 1) return
        if (!numberEqual(vt.property2, 3.1415927)) return

        success = true
    }
}
