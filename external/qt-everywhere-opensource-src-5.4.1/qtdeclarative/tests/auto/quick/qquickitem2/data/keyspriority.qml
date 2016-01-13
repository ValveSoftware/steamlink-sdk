import QtQuick 2.0
import Test 1.0

KeyTestItem {
    focus: true
    Keys.onPressed: keysTestObject.keyPress(event.key, event.text, event.modifiers)
    Keys.onReleased: { keysTestObject.keyRelease(event.key, event.text, event.modifiers); event.accepted = true; }
    Keys.priority: keysTestObject.processLast ? Keys.AfterItem : Keys.BeforeItem

    property int priorityTest: Keys.priority
}
