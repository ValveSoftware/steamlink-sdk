HEADERS += neard/adapter_p.h \
    neard/manager_p.h \
    neard/tag_p.h \
    neard/agent_p.h \
    neard/dbusproperties_p.h \
    neard/dbusobjectmanager_p.h \
    neard/neard_helper_p.h

SOURCES += neard/adapter.cpp \
    neard/manager.cpp \
    neard/tag.cpp \
    neard/agent.cpp \
    neard/dbusproperties.cpp \
    neard/dbusobjectmanager.cpp \
    neard/neard_helper.cpp

OTHER_FILES += neard/org.freedesktop.dbus.objectmanager.xml \
    neard/org.freedesktop.dbus.properties.xml \
    neard/org.neard.Adapter.xml \
    neard/org.neard.Agent.xml \
    neard/org.neard.Manager.xml \
    neard/org.neard.Tag.xml
