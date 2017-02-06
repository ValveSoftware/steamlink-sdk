import QtQuick 2.0
import "{{ServerBaseUrl}}/singleton/remote"

Item {
    property int value1: RemoteSingletonType2.testProp1;
    property string value2: "Test value: " + RemoteSingletonType2.testProp3;
    property variant singletonInstance: RemoteSingletonType2;
}
