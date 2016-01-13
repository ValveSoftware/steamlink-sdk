import QtQuick 1.0

Rectangle {
    id: main
    width: 620; height: 280


    Grid {
        x: 4; y: 4
        spacing: 8
        columns: 4

        Column {
            spacing: 4
            BorderedText { }
            BorderedText { horizontalAlignment: Text.AlignHCenter }
            BorderedText { horizontalAlignment: Text.AlignRight }
        }

        Column {
            spacing: 4
            BorderedText { wrapMode: Text.Wrap }
            BorderedText { horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
            BorderedText { horizontalAlignment: Text.AlignRight; wrapMode: Text.Wrap }
        }

        Column {
            spacing: 4
            BorderedText { wrapMode: Text.Wrap; elide: Text.ElideRight }
            BorderedText { horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; elide: Text.ElideRight }
            BorderedText { horizontalAlignment: Text.AlignRight; wrapMode: Text.Wrap; elide: Text.ElideRight }
        }

        Column {
            spacing: 4
            BorderedText { width: 230; wrapMode: Text.Wrap; elide: Text.ElideRight }
            BorderedText { width: 230; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; elide: Text.ElideRight }
            BorderedText { width: 230; horizontalAlignment: Text.AlignRight; wrapMode: Text.Wrap; elide: Text.ElideRight }
        }

        Column {
            spacing: 4
            BorderedText { width: 120; wrapMode: Text.Wrap; elide: Text.ElideRight }
            BorderedText { width: 120; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; elide: Text.ElideRight }
            BorderedText { width: 120; horizontalAlignment: Text.AlignRight; wrapMode: Text.Wrap; elide: Text.ElideRight }
        }

        Column {
            spacing: 4
            BorderedText { width: 120; wrapMode: Text.Wrap }
            BorderedText { width: 120; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
            BorderedText { width: 120; horizontalAlignment: Text.AlignRight; wrapMode: Text.Wrap }
        }

        Column {
            spacing: 4
            BorderedText { width: 120 }
            BorderedText { width: 120; horizontalAlignment: Text.AlignHCenter }
            BorderedText { width: 120; horizontalAlignment: Text.AlignRight }
        }
    }
}
