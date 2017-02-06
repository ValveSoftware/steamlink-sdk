TEMPLATE = subdirs
SUBDIRS += qserialport qserialportinfo qserialportinfoprivate cmake

!qtConfig(private_tests): SUBDIRS -= \
    qserialportinfoprivate
