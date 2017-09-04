import QtQuick 2.0
import "components"

QtObject {
    id: root
    property int counter
    function increment() {
        counter++
        return counter
    }
}
