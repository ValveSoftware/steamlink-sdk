import QtQuick 2.8

Item {
    property int api: GraphicsInfo.api

    property int shaderType: GraphicsInfo.shaderType
    property int shaderCompilationType: GraphicsInfo.shaderCompilationType
    property int shaderSourceType: GraphicsInfo.shaderSourceType

    property int majorVersion: GraphicsInfo.majorVersion
    property int minorVersion: GraphicsInfo.minorVersion
    property int profile: GraphicsInfo.profile
    property int renderableType: GraphicsInfo.renderableType
}
