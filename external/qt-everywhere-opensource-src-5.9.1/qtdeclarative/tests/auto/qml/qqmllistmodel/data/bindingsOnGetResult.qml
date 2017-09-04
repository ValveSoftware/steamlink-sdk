import QtQuick 2.0

QtObject {
    property ListModel model: ListModel {
        ListElement { modified: false }
        ListElement { modified: false }
        ListElement { modified: false }
        ListElement { modified: false }
        ListElement { modified: false }
    }

    property bool isModified: {
        for (var i = 0; i < model.count; ++i) {
            if (model.get(i).modified)
                return true;
        }
        return false;
    }

    property bool success: false
    Component.onCompleted: {
        // trigger read and setup of property captures
        success = isModified
        model.setProperty(0, "modified", true)
        success = isModified
    }
}
