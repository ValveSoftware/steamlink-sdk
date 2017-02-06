import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Text.RichText
        text: "A table: <table border=2>"
              + "<tr><th>Header 1</th><th>Header 2</th></tr>"
              + "<tr><td>Cell 1</td><td>Cell 2</td></tr>"
              + "<tr><td>Cell 3</td><td>Cell 4</td></tr>"
              + "<tr><td colspan=2>Cell 5</td></tr>"
              + "<tr><td rowspan=2>Cell 6</td><td>Cell 7</tr>"
              + "<tr><td>Cell 8</td></tr>"
    }
}
