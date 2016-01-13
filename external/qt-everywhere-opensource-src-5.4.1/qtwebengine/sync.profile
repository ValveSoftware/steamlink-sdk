%modules = ( # path to module name map
    "QtWebEngine" => "$basedir/src/webengine",
    "QtWebEngineWidgets" => "$basedir/src/webenginewidgets",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
    "QtWebEngine" => "api",
    "QtWebEngineWidgets" => "api",
);
%classnames = (
);

# Module dependencies.
# Every module that is required to build this module should have one entry.
# Each of the module version specifiers can take one of the following values:
#   - A specific Git revision.
#   - any git symbolic ref resolvable from the module's repository (e.g. "refs/heads/master" to track master branch)
#
%dependencies = (
    "qtbase" => "",
    "qtdeclarative" => "",
    "qtxmlpatterns" => "",
# FIXME: take examples out into their own module to avoid a potential circular dependency later ?
    "qtquickcontrols" => "",
);
