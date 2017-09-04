import QtQuick 2.0

Item {
    id: root
    property bool test: false

    property var truck: new vehicle(8);
    property int wheelCount: truck.wheels

    function vehicle(wheels) {
        this.wheels = wheels;
    }

    Component.onCompleted: {
        if (wheelCount != 8) return;

        // not bindable, but wheelCount will update because truck itself changed.
        truck = new vehicle(12);

        if (wheelCount != 12) return;

        test = true;
    }
}
