import QtQuick 1.1
import "../shared" 1.0

Text{
    FontLoader { id: fixedFont; source: "DejaVuSansMono.ttf" }
    font.family: fixedFont.name
    font.pixelSize: 12
}
