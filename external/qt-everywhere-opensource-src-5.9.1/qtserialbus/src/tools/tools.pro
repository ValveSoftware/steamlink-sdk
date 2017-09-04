TEMPLATE = subdirs

qtConfig(commandlineparser):!android|android_app: SUBDIRS += canbusutil
