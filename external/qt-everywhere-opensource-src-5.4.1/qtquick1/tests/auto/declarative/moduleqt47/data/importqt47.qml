import Qt 4.7

Item {
    QtObject {}

    Component { Item {} }

    // Util
    states: [
        State {
            name: "bob"
            AnchorChanges {}
            ParentChange {}
            StateChangeScript {}
            PropertyChanges {}
        }
    ]
    transitions: [
        Transition {
            AnchorAnimation {}
            ColorAnimation {}
            SmoothedAnimation {}
            NumberAnimation {}
            ParallelAnimation {}
            ParentAnimation {}
            PauseAnimation {}
            PropertyAnimation {}
            RotationAnimation {}
            ScriptAction {}
            SequentialAnimation {}
            SpringAnimation {}
            Vector3dAnimation {}
        }
    ]

    Behavior on x {}
    Binding {}
    Connections {}
    FontLoader {}
    ListModel { ListElement {} }
    SystemPalette {}
    Timer {}

    // graphic items
    BorderImage {}
    Column {}
    MouseArea {}
    Flickable {}
    Flipable {}
    Flow {}
    FocusPanel {}
    FocusScope {}
    Rectangle { gradient: Gradient { GradientStop {} } }
    Grid {}
    GridView {}
    Image {}
    ListView {}
    Loader {}
    PathView {
        path: Path {
            PathLine {}
            PathCubic {}
            PathPercent {}
            PathQuad {}
            PathAttribute {}
        }
    }
    Repeater {}
    Rotation {}
    Row {}
    Translate {}
    Scale {}
    Text {}
    TextEdit {}
    TextInput {}
    VisualItemModel {}
    VisualDataModel {}

    Keys.onPressed: console.log("Press")
}
