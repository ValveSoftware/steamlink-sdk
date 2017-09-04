import QtQuick 2.6

ListView {
    id: lv

    // How many rows per page
    property int pageSize: 5

    // The current page number
    property int currentPage: 1

    // How large a single item is
    property int itemSize: 100

    // Arbitrary
    property int totalPages: 5

    height: itemSize * pageSize // display one full page at a time
    width: 500 // arbitrary.
    model: pageSize * totalPages
    delegate: Text {
        height: itemSize
        text: "Item " + (index + 1) + " of " + lv.count
    }

    // contentY should be < 0 to account for header visibility
    onContentYChanged: console.log(contentY)

    headerPositioning: ListView.OverlayHeader
    header: Rectangle {
        height: itemSize
        width: 500
        z: 1000
        visible: false
        color: "black"

        Text {
            anchors.centerIn: parent
            color: "red"
            text: "List header"
        }
    }

    onCurrentPageChanged: {
        lv.positionViewAtIndex((currentPage - 1) * pageSize, ListView.Beginning);
    }
}
