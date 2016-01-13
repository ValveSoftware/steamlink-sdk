TEMPLATE = subdirs

config_geoclue:SUBDIRS += geoclue
config_gypsy:SUBDIRS += gypsy
qtHaveModule(simulator):SUBDIRS += simulator
blackberry:SUBDIRS += blackberry
ios:SUBDIRS += corelocation
android:!android-no-sdk:SUBDIRS += android
winrt:SUBDIRS += winrt

SUBDIRS += \
    positionpoll
