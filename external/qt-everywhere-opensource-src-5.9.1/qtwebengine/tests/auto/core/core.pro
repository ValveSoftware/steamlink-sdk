TEMPLATE = subdirs

SUBDIRS += \
    qwebenginecookiestore \
    qwebengineurlrequestinterceptor \

# QTBUG-60268
boot2qt: SUBDIRS = ""
