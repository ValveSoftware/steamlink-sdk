import QtQuick 2.0
import "singleton/qmldir-error"

Item {
    property int value1: SType2.testProp1;
    property string value2: "Test value: " + SType2.testProp3;
}