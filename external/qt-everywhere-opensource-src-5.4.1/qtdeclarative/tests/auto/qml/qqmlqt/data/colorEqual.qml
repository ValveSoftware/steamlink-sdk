import QtQuick 2.0

QtObject {
    property color color1

    property bool test1a: Qt.colorEqual(color1)
    property bool test1b: Qt.colorEqual('red')

    property bool test1c: Qt.colorEqual(color1, '')
    property bool test1d: Qt.colorEqual('', color1)

    property bool test1e: Qt.colorEqual(color1, 7)
    property bool test1f: Qt.colorEqual(7, color1)

    property var other: ({ name: 'value' })

    property bool test1g: Qt.colorEqual(color1, other)
    property bool test1h: Qt.colorEqual(other, color1)

    property color color2: 'red'

    property bool test2a: Qt.colorEqual(color2, 'red')
    property bool test2b: Qt.colorEqual('red', color2)
    property bool test2c: Qt.colorEqual(color2, '#f00')
    property bool test2d: Qt.colorEqual('#f00', color2)
    property bool test2e: Qt.colorEqual(color2, '#ff0000')
    property bool test2f: Qt.colorEqual('#ff0000', color2)
    property bool test2g: Qt.colorEqual(color2, '#ffff0000')
    property bool test2h: Qt.colorEqual('#ffff0000', color2)
    property bool test2i: Qt.colorEqual(color2, '#80ff0000')
    property bool test2j: Qt.colorEqual('#80ff0000', color2)
    property bool test2k: Qt.colorEqual(color2, 'blue')
    property bool test2l: Qt.colorEqual('blue', color2)
    property bool test2m: Qt.colorEqual(color2, 'darklightmediumgreen')
    property bool test2n: Qt.colorEqual('darklightmediumgreen', color2)

    property color color3: '#f00'

    property bool test3a: Qt.colorEqual(color3, '#f00')
    property bool test3b: Qt.colorEqual('#f00', color3)
    property bool test3c: Qt.colorEqual(color3, '#ff0000')
    property bool test3d: Qt.colorEqual('#ff0000', color3)
    property bool test3e: Qt.colorEqual(color3, '#ffff0000')
    property bool test3f: Qt.colorEqual('#ffff0000', color3)
    property bool test3g: Qt.colorEqual(color3, '#80ff0000')
    property bool test3h: Qt.colorEqual('#80ff0000', color3)
    property bool test3i: Qt.colorEqual(color3, 'red')
    property bool test3j: Qt.colorEqual('red', color3)
    property bool test3k: Qt.colorEqual(color3, 'red')
    property bool test3l: Qt.colorEqual('red', color3)
    property bool test3m: Qt.colorEqual(color3, color2)
    property bool test3n: Qt.colorEqual(color2, color3)

    property color color4: '#80ff0000'

    property bool test4a: Qt.colorEqual(color4, '#80ff0000')
    property bool test4b: Qt.colorEqual('#80ff0000', color4)
    property bool test4c: Qt.colorEqual(color4, '#00ff0000')
    property bool test4d: Qt.colorEqual('#00ff0000', color4)
    property bool test4e: Qt.colorEqual(color4, '#ffff0000')
    property bool test4f: Qt.colorEqual('#ffff0000', color4)
    property bool test4g: Qt.colorEqual(color4, 'red')
    property bool test4h: Qt.colorEqual('red', color4)
    // Note: these fail due to the mismatching precision of their alpha values:
    property bool test4i: Qt.colorEqual(color4, Qt.rgba(1, 0, 0, 0.5))
    property bool test4j: Qt.colorEqual(Qt.rgba(1, 0, 0, 0.5), color4)

    property color color5: 'mediumturquoise'

    property bool test5a: Qt.colorEqual(color5, 'medium' + 'turquoise')
    property bool test5b: Qt.colorEqual('medium' + 'turquoise', color5)
    property bool test5c: Qt.colorEqual(color5, color5)
    property bool test5d: Qt.colorEqual(color5, color3)
    property bool test5e: Qt.colorEqual(color3, color5)

    property bool test6a: Qt.colorEqual('red', 'red')
    property bool test6b: Qt.colorEqual('red', '#f00')
    property bool test6c: Qt.colorEqual('#f00', 'red')
    property bool test6d: Qt.colorEqual('red', 'blue')
    property bool test6e: Qt.colorEqual('blue', 'red')
}
