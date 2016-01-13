import QtQuick 2.0
QtObject {
    signal signal1
    function slot1() {}
    signal signal2
    function slot2() {}

    property int test: 0
    function slot3(a) { console.log(1921); test = a; }
}
