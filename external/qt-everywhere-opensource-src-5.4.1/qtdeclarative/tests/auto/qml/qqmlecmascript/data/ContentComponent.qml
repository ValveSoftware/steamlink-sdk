import QtQuick 2.0

Item {
    property bool validParentChildCount: parent && (parent.children.length > 0)

    onValidParentChildCountChanged: {
        if (!validParentChildCount) console.warn('WARNING: Invalid parent child count')
    }
}
