import QtQuick 2.0

Item {
    property bool test1: false;
    property bool test2: false;

    property int a: 10

    Item {
        id: nested
        property int a: 11

        function mynestedfunction() {
            return a;
        }
    }

    function myouterfunction() {
        return a;
    }

    Component.onCompleted: {
        test1 = (myouterfunction() == 10);
        test2 = (nested.mynestedfunction() == 11);
    }
}

