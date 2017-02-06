import Test 1.0
import QtQuick 2.0

BindingsSpliceCorrectlyType {
    property bool test: false

    property bool italicProperty: false

    font.italic: italicProperty

    Component.onCompleted: {
        // Test initial state
        if (font.italic != false) return;
        if (font.bold != false) return;

        // Test italic binding worked
        italicProperty = true;

        if (font.italic != true) return;
        if (font.bold != false) return;

        // Test bold binding worked
        boldProperty = true;
        if (font.italic != true) return;
        if (font.bold != true) return;

        test = true;
    }
}
