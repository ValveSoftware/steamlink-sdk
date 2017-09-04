import QtQuick 2.4

Item {
    function hint() {
    }

    Connections {
        target: BlaBlaBla
        onHint: hint();
    }
}
