%modules = ( # path to module name map
    "QtWebEngine" => "$basedir/src/webengine",
    "QtWebEngineWidgets" => "$basedir/src/webenginewidgets",
    "QtWebEngineCore" => "$basedir/src/core",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
    "QtWebEngine" => "api",
    "QtWebEngineWidgets" => "api",
    "QtWebEngineCore" => "api",
);
%classnames = (
);
