#Subdirectiores are defined here, because qt creator doesn't handle nested include(foo.pri) chains very well.

INCLUDEPATH += $$PWD \
    $$PWD/valueaxis \
    $$PWD/barcategoryaxis \
    $$PWD/categoryaxis \
    $$PWD/logvalueaxis

SOURCES += \
    $$PWD/chartaxiselement.cpp \
    $$PWD/cartesianchartaxis.cpp \
    $$PWD/qabstractaxis.cpp \
    $$PWD/verticalaxis.cpp \
    $$PWD/horizontalaxis.cpp \
    $$PWD/valueaxis/chartvalueaxisx.cpp \
    $$PWD/valueaxis/chartvalueaxisy.cpp \
    $$PWD/valueaxis/qvalueaxis.cpp \
    $$PWD/barcategoryaxis/chartbarcategoryaxisx.cpp \
    $$PWD/barcategoryaxis/chartbarcategoryaxisy.cpp \
    $$PWD/barcategoryaxis/qbarcategoryaxis.cpp \
    $$PWD/categoryaxis/chartcategoryaxisx.cpp \
    $$PWD/categoryaxis/chartcategoryaxisy.cpp \
    $$PWD/categoryaxis/qcategoryaxis.cpp \
    $$PWD/logvalueaxis/chartlogvalueaxisx.cpp \
    $$PWD/logvalueaxis/chartlogvalueaxisy.cpp \
    $$PWD/logvalueaxis/qlogvalueaxis.cpp

PRIVATE_HEADERS += \
    $$PWD/chartaxiselement_p.h \
    $$PWD/cartesianchartaxis_p.h \
    $$PWD/qabstractaxis_p.h \
    $$PWD/verticalaxis_p.h \
    $$PWD/horizontalaxis_p.h \
    $$PWD/linearrowitem_p.h \
    $$PWD/valueaxis/chartvalueaxisx_p.h \
    $$PWD/valueaxis/chartvalueaxisy_p.h \
    $$PWD/valueaxis/qvalueaxis_p.h \
    $$PWD/barcategoryaxis/chartbarcategoryaxisx_p.h \
    $$PWD/barcategoryaxis/chartbarcategoryaxisy_p.h \
    $$PWD/barcategoryaxis/qbarcategoryaxis_p.h \
    $$PWD/categoryaxis/chartcategoryaxisx_p.h \
    $$PWD/categoryaxis/chartcategoryaxisy_p.h \
    $$PWD/categoryaxis/qcategoryaxis_p.h \
    $$PWD/logvalueaxis/chartlogvalueaxisx_p.h \
    $$PWD/logvalueaxis/chartlogvalueaxisy_p.h \
    $$PWD/logvalueaxis/qlogvalueaxis_p.h

PUBLIC_HEADERS += \
    $$PWD/qabstractaxis.h \
    $$PWD/valueaxis/qvalueaxis.h \
    $$PWD/barcategoryaxis/qbarcategoryaxis.h \
    $$PWD/categoryaxis/qcategoryaxis.h \
    $$PWD/logvalueaxis/qlogvalueaxis.h \

# polar
SOURCES += \
    $$PWD/polarchartaxis.cpp \
    $$PWD/polarchartaxisangular.cpp \
    $$PWD/polarchartaxisradial.cpp \
    $$PWD/valueaxis/polarchartvalueaxisangular.cpp \
    $$PWD/valueaxis/polarchartvalueaxisradial.cpp \
    $$PWD/logvalueaxis/polarchartlogvalueaxisangular.cpp \
    $$PWD/logvalueaxis/polarchartlogvalueaxisradial.cpp \
    $$PWD/categoryaxis/polarchartcategoryaxisangular.cpp \
    $$PWD/categoryaxis/polarchartcategoryaxisradial.cpp

PRIVATE_HEADERS += \
    $$PWD/polarchartaxis_p.h \
    $$PWD/polarchartaxisangular_p.h \
    $$PWD/polarchartaxisradial_p.h \
    $$PWD/valueaxis/polarchartvalueaxisangular_p.h \
    $$PWD/valueaxis/polarchartvalueaxisradial_p.h \
    $$PWD/logvalueaxis/polarchartlogvalueaxisangular_p.h \
    $$PWD/logvalueaxis/polarchartlogvalueaxisradial_p.h \
    $$PWD/categoryaxis/polarchartcategoryaxisangular_p.h \
    $$PWD/categoryaxis/polarchartcategoryaxisradial_p.h

!contains(QT_COORD_TYPE, float): {
INCLUDEPATH += \
    $$PWD/datetimeaxis

SOURCES += \
    $$PWD/datetimeaxis/chartdatetimeaxisx.cpp \
    $$PWD/datetimeaxis/chartdatetimeaxisy.cpp \
    $$PWD/datetimeaxis/qdatetimeaxis.cpp \
    $$PWD/datetimeaxis/polarchartdatetimeaxisangular.cpp \
    $$PWD/datetimeaxis/polarchartdatetimeaxisradial.cpp

PRIVATE_HEADERS += \
    $$PWD/datetimeaxis/chartdatetimeaxisx_p.h \
    $$PWD/datetimeaxis/chartdatetimeaxisy_p.h \
    $$PWD/datetimeaxis/qdatetimeaxis_p.h \
    $$PWD/datetimeaxis/polarchartdatetimeaxisangular_p.h \
    $$PWD/datetimeaxis/polarchartdatetimeaxisradial_p.h

PUBLIC_HEADERS += \
    $$PWD/datetimeaxis/qdatetimeaxis.h
}

