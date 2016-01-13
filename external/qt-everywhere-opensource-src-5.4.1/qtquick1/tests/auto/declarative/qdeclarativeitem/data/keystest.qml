import QtQuick 1.0

Item {
    focus: true

    property bool isEnabled: Keys.enabled

    Keys.onPressed: keysTestObject.keyPress(event.key, event.text, event.modifiers)
    Keys.onReleased: { keysTestObject.keyRelease(event.key, event.text, event.modifiers); event.accepted = true; }
    Keys.onReturnPressed: keysTestObject.keyPress(event.key, "Return", event.modifiers)
    Keys.onDigit0Pressed: keysTestObject.keyPress(event.key, event.text, event.modifiers)
    Keys.onDigit9Pressed: { event.accepted = false; keysTestObject.keyPress(event.key, event.text, event.modifiers) }
    Keys.onTabPressed: keysTestObject.keyPress(event.key, "Tab", event.modifiers)
    Keys.onBacktabPressed: keysTestObject.keyPress(event.key, "Backtab", event.modifiers)
    Keys.forwardTo: [ item2 ]
    Keys.enabled: enableKeyHanding

    Item {
        id: item2
        visible: forwardeeVisible
        Keys.onPressed: keysTestObject.forwardedKey(event.key)
        Keys.onReleased: keysTestObject.forwardedKey(event.key)
    }
}
