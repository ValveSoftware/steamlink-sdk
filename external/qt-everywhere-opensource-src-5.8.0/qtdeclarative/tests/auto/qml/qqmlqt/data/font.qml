import QtQuick 2.0

QtObject {
    property variant test1: Qt.font({ family: "Arial", pointSize: 22 });
    property variant test2: Qt.font({ family: "Arial", pointSize: 20, weight: Font.DemiBold, italic: true });
    property variant test3: Qt.font("Arial", 22);
    property variant test4: Qt.font({ something: "Arial", other: 22 });
}
