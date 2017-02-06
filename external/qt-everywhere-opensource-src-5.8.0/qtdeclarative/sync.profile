%modules = ( # path to module name map
    "QtQml" => "$basedir/src/qml",
    "QtQuick" => "$basedir/src/quick",
    "QtQuickWidgets" => "$basedir/src/quickwidgets",
    "QtQuickParticles" => "$basedir/src/particles",
    "QtQuickTest" => "$basedir/src/qmltest",
    "QtQmlDevTools" => "$basedir/src/qmldevtools",
    "QtPacketProtocol" => "$basedir/src/plugins/qmltooling/packetprotocol",
    "QtQmlDebug" => "$basedir/src/qmldebug",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
    "QtQmlDevTools" => "../qml/parser;../qml/jsruntime;../qml/qml/ftw;../qml/compiler;../qml/memory;.",
);
%deprecatedheaders = (
);
