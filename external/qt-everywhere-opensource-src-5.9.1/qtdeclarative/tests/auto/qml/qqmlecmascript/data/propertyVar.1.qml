import QtQuick 2.0

Item {
    id: root
    property bool test: false

    property var car: new vehicle(4);
    property int wheelCount: car.wheels

    function vehicle(wheels) {
        this.wheels = wheels;
    }

    Component.onCompleted: {
        car.wheels = 6;  // not bindable, wheelCount shouldn't update

        if (car.wheels != 6) return;
        if (wheelCount != 4) return;

        test = true;
    }
}
