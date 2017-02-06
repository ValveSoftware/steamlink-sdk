%modules = ( # path to module name map
    "QtMultimedia" => "$basedir/src/multimedia",
    "QtMultimediaWidgets" => "$basedir/src/multimediawidgets",
    "QtMultimediaQuick_p" => "$basedir/src/qtmultimediaquicktools",
);

%moduleheaders = ( # restrict the module headers to those found in relative path
);

%classnames = (
    "qaudio.h" => "QAudio",
    "qmediametadata.h" => "QMediaMetaData",
    "qmultimedia.h" => "QMultimedia"
);
%deprecatedheaders = (
    "QtMultimedia" =>  {
        "qtmultimediadefs.h" => "QtMultimedia/qtmultimediaglobal.h"
    },
);
