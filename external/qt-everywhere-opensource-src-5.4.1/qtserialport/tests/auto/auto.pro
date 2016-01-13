TEMPLATE = subdirs
SUBDIRS += qserialport qserialportinfo qserialportinfoprivate cmake

!contains(QT_CONFIG, private_tests): SUBDIRS -= \
    qserialportinfoprivate
