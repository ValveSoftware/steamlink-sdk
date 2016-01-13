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
              + "<tr><td>Cell 1</td><td>Cell 2 <img height=16 width=16 src=\"data/logo.png\" /></td></tr>"
              + "<tr><td>Cell <img height=16 width=16 src=\"data/logo.png\" /> 3</td><td>Cell 4</td></tr>"
              + "<tr><td colspan=2>Ce<img height=16 width=16 src=\"data/logo.png\" />ll 5</td></tr>"
              + "<tr><td rowspan=2> <img height=16 width=16 src=\"data/logo.png\" />Cell 6</td><td>Cell 7</tr>"
              + "<tr><td>Cell 8 <img height=16 width=16 src=\"data/logo.png\" /></td></tr>"
    }
}
