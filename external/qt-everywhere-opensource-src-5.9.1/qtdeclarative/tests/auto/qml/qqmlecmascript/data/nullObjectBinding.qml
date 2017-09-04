import QtQuick 2.0

QtObject {
    property QtObject test
    test: if (1) model
    property ListModel model
}

