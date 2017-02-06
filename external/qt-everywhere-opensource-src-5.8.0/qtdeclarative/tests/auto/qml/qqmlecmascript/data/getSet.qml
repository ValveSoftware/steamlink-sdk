import QtQuick 2.0

QtObject {
    function get(x) { return 1; }
    function set(x) { return 1; }
    function code() {
        var get = 0;
        var set = 1;
        var o = {
            get foo() { return 2; },
            set foo(x) { 1; }
        }
    }
}
