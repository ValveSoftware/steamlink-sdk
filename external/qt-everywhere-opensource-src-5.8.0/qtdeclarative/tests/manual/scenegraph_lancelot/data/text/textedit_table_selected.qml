import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        verticalAlignment: Text.AlignBottom
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Text.RichText
        Component.onCompleted: textEdit.selectAll()
        text:
            "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n" +
            "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n" +
            "<body >\n" +
            "<table >\n" +
            "<tr >\n" +
            "<td >\n" +
            "Cell 1\n" +
            "</td>\n" +
            "</tr>\n" +
            "<tr >\n" +
            "<td >\n" +
            "Cell 2\n" +
            "</td>\n" +
            "</tr>\n" +
            "<tr >\n" +
            "<td >\n" +
            "Cell 3\n" +
            "</td>\n" +
            "</tr>\n" +
            "</table>\n" +
            "</body>\n" +
            "</html>\n"

    }
}
