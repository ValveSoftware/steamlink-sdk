%modules = ( # path to module name map
    "QtWaylandCompositor" => "$basedir/src/compositor",
    "QtWaylandClient" => "$basedir/src/client",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
);
%classnames = (
);
%deprecatedheaders = (
    "QtWaylandClient" =>  {
        "qwaylandclientexport.h" => "QtWaylandClient/qtwaylandclientglobal.h"
    },
    "QtWaylandCompositor" =>  {
        "qwaylandexport.h" => "QtWaylandCompositor/qtwaylandcompositorglobal.h"
    }
);
%classnames = (
    "qwaylandquickextension.h" => "QWaylandQuickExtension",
);
