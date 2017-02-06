%modules = ( # path to module name map
    "Qt3DCore" => "$basedir/src/core",
    "Qt3DRender" => "$basedir/src/render",
    "Qt3DQuick" => "$basedir/src/quick3d/quick3d",
    "Qt3DQuickRender" => "$basedir/src/quick3d/quick3drender",
    "Qt3DInput" => "$basedir/src/input",
    "Qt3DQuickInput" => "$basedir/src/quick3d/quick3dinput",
    "Qt3DLogic" => "$basedir/src/logic",
    "Qt3DExtras" => "$basedir/src/extras",
    "Qt3DQuickExtras" => "$basedir/src/quick3d/quick3dextras",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
);

# Force generation of camel case headers for classes inside Qt3D* namespaces
$publicclassregexp = "Qt3D.*::.+";
