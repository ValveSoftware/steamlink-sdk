import QtQuick 2.0
import "qtbug_11600.js" as Test

QtObject {
    id: goo

    property bool test: undefined == goo.Test
}
