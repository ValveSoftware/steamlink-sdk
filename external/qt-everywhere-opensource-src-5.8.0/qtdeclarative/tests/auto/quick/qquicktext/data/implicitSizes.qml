import QtQuick 2.0

Rectangle {
    width: 200
    height: column.height

    Column {
        id: column
        Text {
            id: reference
            objectName: "reference"

            wrapMode: Text.Wrap
            elide: Text.ElideRight

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
        }
        Text {
            id: fixedWidthAndHeight
            objectName: "fixedWidthAndHeight"

            width: 100
            height: 100

            wrapMode: Text.Wrap
            elide: Text.ElideRight

            text: reference.text
        }

        Text {
            id: implicitWidthFixedHeight
            objectName: "implicitWidthFixedHeight"

            height: 100

            wrapMode: Text.Wrap
            elide: Text.ElideRight

            text: reference.text
        }
        Text {
            id: fixedWidthImplicitHeight
            objectName: "fixedWidthImplicitHeight"

            width: 100

            wrapMode: Text.Wrap
            elide: Text.ElideRight

            text: reference.text
        }
        Text {
            id: cappedWidthAndHeight
            objectName: "cappedWidthAndHeight"

            width: Math.min(100, implicitWidth)
            height: Math.min(100, implicitHeight)

            wrapMode: Text.Wrap
            elide: Text.ElideRight

            text: reference.text
        }
        Text {
            id: cappedWidthFixedHeight
            objectName: "cappedWidthFixedHeight"

            width: Math.min(100, implicitWidth)
            height: 100

            wrapMode: Text.Wrap
            elide: Text.ElideRight

            text: reference.text
        }
        Text {
            id: fixedWidthCappedHeight
            objectName: "fixedWidthCappedHeight"

            width: 100
            height: Math.min(100, implicitHeight)

            wrapMode: Text.Wrap
            elide: Text.ElideRight

            text: reference.text
        }
    }
}
