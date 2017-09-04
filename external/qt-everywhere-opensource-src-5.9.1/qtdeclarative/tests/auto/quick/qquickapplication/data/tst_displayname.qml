import QtQuick 2.0;

Item {
    id: root;
    property string displayName: Qt.application.displayName;
    function updateDisplayName(name) { Qt.application.displayName = name; }
}
