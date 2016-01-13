import QtQuick 2.0
import "data" as Subdirectory

SameDir {
    property QtObject other: Subdirectory.SubDir {}
}
