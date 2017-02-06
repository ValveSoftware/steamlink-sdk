import QtQuick 2.0

Rectangle {
    id: main
    width: 800; height: 600

    property real off: 0

    Text {
        id: myText
        objectName: "myText"
        wrapMode: Text.WordWrap
        font.pixelSize: 14
        textFormat: Text.StyledText
        focus: true

        text: "<b>Lorem ipsum</b> dolor sit amet, consectetur adipiscing elit. Integer at ante dui. Sed eu egestas est.
        <br/><p><i>Maecenas nec libero leo. Sed ac leo eget ipsum ultricies viverra sit amet eu orci. Praesent et tortor risus, viverra accumsan sapien. Sed faucibus eleifend lectus, sed euismod urna porta eu. Aenean ultricies lectus ut orci dictum quis convallis nisi ultrices. Nunc elit mi, iaculis a porttitor rutrum, venenatis malesuada nisi. Suspendisse turpis quam, euismod non imperdiet et, rutrum nec ligula. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam semper tristique metus eu sodales. Integer eget risus ipsum. Quisque ut risus ut nulla tristique volutpat at sit amet nisl. Aliquam pulvinar auctor diam nec bibendum.</i><br/><p>Quisque luctus sapien id arcu volutpat pharetra. Praesent pretium imperdiet euismod. Integer fringilla rhoncus condimentum. Quisque sit amet ornare nulla. Cras sapien augue, sagittis a dictum id, suscipit et nunc. Cras vitae augue in enim elementum venenatis sed nec risus. Sed nisi quam, mollis quis auctor ac, vestibulum in neque. Vivamus eu justo risus. Suspendisse vel mollis est. Vestibulum gravida interdum mi, in molestie neque gravida in. Donec nibh odio, mattis facilisis vulputate et, scelerisque ut felis. Sed ornare eros nec odio aliquam eu varius augue adipiscing. Vivamus sit amet massa dapibus sapien pulvinar consectetur a sit amet felis. Cras non mi id libero dictum iaculis id dignissim eros. Praesent eget enim dui, sed bibendum neque. Ut interdum nisl id leo malesuada ornare. Pellentesque id nisl eu odio volutpat posuere et at massa. Pellentesque nec lorem justo. Integer sem urna, pharetra sed sagittis vitae, condimentum ac felis. Ut vitae sapien ac tortor adipiscing pharetra. Cras tristique urna tempus ante volutpat eleifend non eu ligula. Mauris sodales nisl et lorem tristique sodales. Mauris arcu orci, vehicula semper cursus ac, dapibus ut mi."

        onLineLaidOut: {
            line.width = line.number * 15
            if (line.number === 30 || line.number === 60) {
                main.off = line.y
            }
            if (line.number >= 30) {
                line.x = line.width + 30
                line.y -= main.off
            }
            if (line.number >= 60) {
                line.x = line.width * 2 + 60
                line.height = 20
            }
        }
    }
}
