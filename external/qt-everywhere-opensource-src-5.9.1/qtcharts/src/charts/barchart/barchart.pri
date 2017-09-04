#Subdirectiores are defined here, because qt creator doesn't handle nested include(foo.pri) chains very well.

INCLUDEPATH += $$PWD \
    $$PWD/vertical/bar \
    $$PWD/vertical/stacked \
    $$PWD/vertical/percent \
    $$PWD/horizontal/bar \
    $$PWD/horizontal/stacked \
    $$PWD/horizontal/percent

SOURCES += \
    $$PWD/bar.cpp \
    $$PWD/abstractbarchartitem.cpp \
    $$PWD/qabstractbarseries.cpp \  
    $$PWD/qbarset.cpp \
    $$PWD/qbarmodelmapper.cpp \
    $$PWD/qvbarmodelmapper.cpp \
    $$PWD/qhbarmodelmapper.cpp \
    $$PWD/vertical/bar/qbarseries.cpp \
    $$PWD/vertical/bar/barchartitem.cpp \
    $$PWD/vertical/stacked/qstackedbarseries.cpp \
    $$PWD/vertical/stacked/stackedbarchartitem.cpp \
    $$PWD/vertical/percent/qpercentbarseries.cpp \
    $$PWD/vertical/percent/percentbarchartitem.cpp \
    $$PWD/horizontal/bar/qhorizontalbarseries.cpp \
    $$PWD/horizontal/bar/horizontalbarchartitem.cpp \
    $$PWD/horizontal/stacked/qhorizontalstackedbarseries.cpp \
    $$PWD/horizontal/stacked/horizontalstackedbarchartitem.cpp \
    $$PWD/horizontal/percent/qhorizontalpercentbarseries.cpp \
    $$PWD/horizontal/percent/horizontalpercentbarchartitem.cpp

PRIVATE_HEADERS += \
    $$PWD/bar_p.h \
    $$PWD/qbarset_p.h \
    $$PWD/abstractbarchartitem_p.h \
    $$PWD/qabstractbarseries_p.h \
    $$PWD/qbarmodelmapper_p.h \
    $$PWD/vertical/bar/qbarseries_p.h \
    $$PWD/vertical/bar/barchartitem_p.h \
    $$PWD/vertical/stacked/qstackedbarseries_p.h \
    $$PWD/vertical/stacked/stackedbarchartitem_p.h \
    $$PWD/vertical/percent/qpercentbarseries_p.h \
    $$PWD/vertical/percent/percentbarchartitem_p.h \
    $$PWD/horizontal/bar/qhorizontalbarseries_p.h \
    $$PWD/horizontal/bar/horizontalbarchartitem_p.h \
    $$PWD/horizontal/stacked/qhorizontalstackedbarseries_p.h \
    $$PWD/horizontal/stacked/horizontalstackedbarchartitem_p.h \
    $$PWD/horizontal/percent/qhorizontalpercentbarseries_p.h \
    $$PWD/horizontal/percent/horizontalpercentbarchartitem_p.h

PUBLIC_HEADERS += \
    $$PWD/qabstractbarseries.h \
    $$PWD/qbarset.h \
    $$PWD/qbarmodelmapper.h \
    $$PWD/qvbarmodelmapper.h \
    $$PWD/qhbarmodelmapper.h \
    $$PWD/vertical/bar/qbarseries.h \
    $$PWD/vertical/stacked/qstackedbarseries.h \
    $$PWD/vertical/percent/qpercentbarseries.h \
    $$PWD/horizontal/bar/qhorizontalbarseries.h \
    $$PWD/horizontal/stacked/qhorizontalstackedbarseries.h \
    $$PWD/horizontal/percent/qhorizontalpercentbarseries.h

