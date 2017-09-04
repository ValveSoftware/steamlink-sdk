import QtQml 2.0

QtObject {
    property string originalName
    property string originalVersion
    property string originalOrganization
    property string originalDomain
    property string currentName: Qt.application.name
    property string currentVersion: Qt.application.version
    property string currentOrganization: Qt.application.organization
    property string currentDomain: Qt.application.domain
    property QtObject applicationInstance: Qt.application
    Component.onCompleted: {
        originalName = Qt.application.name
        originalVersion = Qt.application.version
        originalOrganization = Qt.application.organization
        originalDomain = Qt.application.domain
        Qt.application.name = "Test B"
        Qt.application.version = "0.0B"
        Qt.application.organization = "Org B"
        Qt.application.domain = "b.org"
    }
}
