import QtQuick 2.0

QtObject {
    id: root
    onDestroyed: {} //this should cause an error (destroyed should be hidden)
}
