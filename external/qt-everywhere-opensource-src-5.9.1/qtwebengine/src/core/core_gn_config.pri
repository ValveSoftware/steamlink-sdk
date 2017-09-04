CONFIG = gn_generator $$CONFIG
GN_SRC_DIR = $$PWD
GN_FILE = $$OUT_PWD/$$getConfigDir()/BUILD.gn
GN_FIND_MOCABLES_SCRIPT = $$shell_path($$QTWEBENGINE_ROOT/tools/scripts/gn_find_mocables.py)
GN_RUN_BINARY_SCRIPT = $$shell_path($$QTWEBENGINE_ROOT/tools/scripts/gn_run_binary.py)
GN_IMPORTS =  $$PWD/qtwebengine.gni
GN_INCLUDES = $$PWD/qtwebengine_sources.gni $$PWD/qtwebengine_resources.gni
GN_CREATE_PRI = true
QMAKE_INTERNAL_INCLUDED_FILES = $$GN_IMPORTS $$GN_INCLUDES $$GN_FILE

