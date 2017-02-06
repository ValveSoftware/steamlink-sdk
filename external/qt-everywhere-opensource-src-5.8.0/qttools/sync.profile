%modules = ( # path to module name map
    "QtCLucene" => "$basedir/src/assistant/clucene",
    "QtHelp" => "$basedir/src/assistant/help",
    "QtUiTools" => "$basedir/src/designer/src/uitools",
    "QtUiPlugin" => "$basedir/src/designer/src/uiplugin",
    "QtDesigner" => "$basedir/src/designer/src/lib",
    "QtDesignerComponents" => "$basedir/src/designer/src/components/lib",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
);
%classnames = (
);
%deprecatedheaders = (
    "QtDesigner" => {
        "customwidget.h" => "QtUiPlugin/customwidget.h",
        "QDesignerCustomWidgetInterface" => "QtUiPlugin/QDesignerCustomWidgetInterface",
        "QDesignerCustomWidgetCollectionInterface" => "QtUiPlugin/QDesignerCustomWidgetCollectionInterface",
        "qdesignerexportwidget.h" => "QtUiPlugin/qdesignerexportwidget.h",
        "QDesignerExportWidget" => "QtUiPlugin/QDesignerExportWidget"
    }
);
