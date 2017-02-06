import QtQuick 2.0

Rectangle {
    id: main
    width: 320
    height: 418

    property int yOffset: 0

    Component.onCompleted: myText.height = height

    Text {
        id: myText
        objectName: "myText"
        width: parent.width
        height: 0
        wrapMode: Text.WordWrap
        font.pointSize: 14
        focus: true

        text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit.
Integer at ante dui Curabitur ante est, pulvinar quis adipiscing a, iaculis id ipsum. Nunc blandit
condimentum odio vel egestas. in ipsum lacinia sit amet
mattis orci interdum. Quisque vitae accumsan lectus. Ut nisi turpis,
sollicitudin ut dignissim id, fermentum ac est. Maecenas nec libero leo. Sed ac
mattis orci interdum. Quisque vitae accumsan lectus. Ut nisi turpis,
sollicitudin ut dignissim id, fermentum ac est. Maecenas nec libero leo. Sed ac
leo eget ipsum ultricies viverra sit amet eu orci. Praesent et tortor risus,
viverra accumsan sapien. Sed faucibus eleifend lectus, sed euismod urna porta
eu. Quisque vitae accumsan lectus."

        onLineLaidOut: {
            line.width = width / 2

            if (line.y + line.height >= height) {
                if (main.yOffset === 0)
                    main.yOffset = line.y
                line.y -= main.yOffset
                line.x = width / 2
            } else {
                main.yOffset = 0
            }
        }
    }
}
