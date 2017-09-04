import QtQuick 2.0

Item {
    Item {
        id: item
    }

    SpringAnimation {
        target: item; property: "x"
        to: 1.44; velocity: 0.9
        spring: 1.0; damping: 0.5
        epsilon: 0.25; modulus: 360.0
        mass: 2.0;
        running: true;
    }
}
