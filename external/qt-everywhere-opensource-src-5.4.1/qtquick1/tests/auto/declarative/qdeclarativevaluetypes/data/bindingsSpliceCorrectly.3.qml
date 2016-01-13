import Test 1.0
import QtQuick 1.0

BindingsSpliceCorrectlyType {
    property bool test: false

    property bool italicProperty: false
    property bool boldProperty2: false

    font.italic: italicProperty
    font.bold: boldProperty2

    Component.onCompleted: {
        // Test initial state
        if (font.italic != false) return;
        if (font.bold != false) return;

        // Test italic binding worked
        italicProperty = true;

        if (font.italic != true) return;
        if (font.bold != false) return;

        // Test bold binding was overridden
        boldProperty = true;
        if (font.italic != true) return;
        if (font.bold != false) return;

        boldProperty2 = true;
        if (font.italic != true) return;
        if (font.bold != true) return;

        test = true;
    }
}

