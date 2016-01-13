# Add more folders to ship with the application, here
folder_01.source = qml/minehunt
folder_01.target = qml
DEPLOYMENTFOLDERS = folder_01

# Additional import path used to resolve QML modules in Creator's code model
QML_IMPORT_PATH =

# If your application uses the Qt Mobility libraries, uncomment the following
# lines and add the respective components to the MOBILITY variable.
# CONFIG += mobility
# MOBILITY +=

# Speed up launching on MeeGo/Harmattan when using applauncherd daemon
# CONFIG += qdeclarative-boostable

HEADERS += minehunt.h
SOURCES += main.cpp minehunt.cpp
RESOURCES = minehunt.qrc

# Please do not modify the following two lines. Required for deployment.
desktopInstallPrefix = $$[QT_INSTALL_EXAMPLES]/declarative/demos/minehunt
exists(qmlapplicationviewer/qmlapplicationviewer.pri):include(qmlapplicationviewer/qmlapplicationviewer.pri)
else:include(../../helper/qmlapplicationviewer/qmlapplicationviewer.pri)
qtcAddDeployment()
