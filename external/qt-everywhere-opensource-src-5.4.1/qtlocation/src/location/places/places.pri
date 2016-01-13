INCLUDEPATH += places

PUBLIC_HEADERS += \
    places/placemacro.h \
#data classes
    places/qplace.h \
    places/qplaceattribute.h \
    places/qplacecontactdetail.h \
    places/qplacecategory.h \
    places/qplacecontent.h \
    places/qplacecontentreply.h \
    places/qplaceeditorial.h \
    places/qplaceimage.h \
    places/qplaceicon.h \
    places/qplaceratings.h \
    places/qplacereview.h \
    places/qplacesupplier.h \
    places/qplaceuser.h \
#result
    places/qplacesearchresult.h \
    places/qplaceresult.h \
    places/qplaceproposedsearchresult.h \
#request classes
    places/qplacecontentrequest.h \
    places/qplacematchrequest.h \
    places/qplacesearchrequest.h \
#reply classes
    places/qplacereply.h \
    places/qplacedetailsreply.h \
    places/qplaceidreply.h \
    places/qplacematchreply.h \
    places/qplacesearchreply.h \
    places/qplacesearchsuggestionreply.h \
    places/unsupportedreplies_p.h \
#manager and engine
    places/qplacemanager.h \
    places/qplacemanagerengine.h

PRIVATE_HEADERS += \
    places/qplace_p.h \
    places/qplaceattribute_p.h \
    places/qplacecategory_p.h \
    places/qplacecontent_p.h \
    places/qplacecontactdetail_p.h \
    places/qplaceeditorial_p.h \
    places/qplaceicon_p.h \
    places/qplaceimage_p.h \
    places/qplaceratings_p.h \
    places/qplaceresult_p.h \
    places/qplaceproposedsearchresult_p.h \
    places/qplacereview_p.h \
    places/qplacesupplier_p.h \
    places/qplacesearchresult_p.h \
    places/qplacereply_p.h \
    places/qplacemanagerengine_p.h \
    places/qplacecontentrequest_p.h \
    places/qplaceuser_p.h

SOURCES += \
#data classes
    places/qplace.cpp \
    places/qplaceattribute.cpp \
    places/qplacecategory.cpp \
    places/qplacecontactdetail.cpp \
    places/qplacecontent.cpp \
    places/qplacecontentreply.cpp \
    places/qplaceeditorial.cpp \
    places/qplaceuser.cpp \
#result
    places/qplaceicon.cpp \
    places/qplaceimage.cpp \
    places/qplaceratings.cpp \
    places/qplacereview.cpp \
    places/qplaceidreply.cpp \
    places/qplacesupplier.cpp \
#result
    places/qplacesearchresult.cpp \
    places/qplaceresult.cpp \
    places/qplaceproposedsearchresult.cpp \
#request classes
    places/qplacecontentrequest.cpp \
    places/qplacematchrequest.cpp \
    places/qplacesearchrequest.cpp \
#reply classes
    places/qplacereply.cpp \
    places/qplacedetailsreply.cpp \
    places/qplacematchreply.cpp \
    places/qplacesearchreply.cpp \
    places/qplacesearchsuggestionreply.cpp \
#manager and engine
    places/qplacemanager.cpp \
    places/qplacemanagerengine.cpp

