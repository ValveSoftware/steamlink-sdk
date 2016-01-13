import QtQuick 2.0
import "script.js" as JS

VMEExtendVMEComponent {
    function getSomething() { return JS.getSomething() }
}
