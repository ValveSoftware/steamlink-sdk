TARGET = qtt9write_db

CONFIG += static

T9WRITE_LDBS = $$files(databases/XT9_LDBs/*.ldb)

T9WRITE_RESOURCE_FILES = \
    databases/HWR_LatinCG/_databas_le.bin \
    $$T9WRITE_LDBS

# Note: Compression is disabled, because the resource is accessed directly from the memory
QMAKE_RESOURCE_FLAGS += -no-compress

include(../../generateresource.pri)

RESOURCES += $$generate_resource(t9write_db.qrc, $$T9WRITE_RESOURCE_FILES)

load(qt_helper_lib)

# Needed for resources
CONFIG += qt
