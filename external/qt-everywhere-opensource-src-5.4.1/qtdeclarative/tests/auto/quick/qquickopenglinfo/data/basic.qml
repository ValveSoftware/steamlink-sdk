import QtQuick 2.4

Item {
    property int majorVersion: OpenGLInfo.majorVersion
    property int minorVersion: OpenGLInfo.minorVersion
    property int profile: OpenGLInfo.profile
    property int renderableType: OpenGLInfo.renderableType
}
