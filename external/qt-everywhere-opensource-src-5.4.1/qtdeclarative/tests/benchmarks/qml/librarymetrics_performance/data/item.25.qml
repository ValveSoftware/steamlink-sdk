import QtQuick 2.0
import ModuleApi 1.0 as ModApi

Item {
    property int a: ModApi.ModuleApi.intFunc()
}
