LIBS += -framework StoreKit -framework Foundation
INCLUDEPATH += $$PWD
HEADERS += \
    $$PWD/qmacinapppurchasebackend_p.h \
    $$PWD/qmacinapppurchasetransaction_p.h \
    $$PWD/qmacinapppurchaseproduct_p.h

OBJECTIVE_SOURCES += \
    $$PWD/qmacinapppurchasebackend.mm \
    $$PWD/qmacinapppurchaseproduct.mm \
    $$PWD/qmacinapppurchasetransaction.mm
