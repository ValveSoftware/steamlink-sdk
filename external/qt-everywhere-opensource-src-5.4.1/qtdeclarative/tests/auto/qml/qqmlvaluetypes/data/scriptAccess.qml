import QtQuick 2.0
import Test 1.0

MyTypeObject {
    property int valuePre;
    property int valuePost;

    Component.onCompleted: { valuePre = rect.x; rect.x = 19; valuePost = rect.x; }
}
