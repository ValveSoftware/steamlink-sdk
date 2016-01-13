import QtQuick 1.0

QtObject {
    property bool hd: true

    property real test: ((hd ? 100 : 20) + 0)
}
