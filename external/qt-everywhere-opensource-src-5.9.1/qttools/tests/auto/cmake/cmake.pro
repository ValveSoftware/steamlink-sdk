
# Cause make to do nothing.
TEMPLATE = subdirs

qtHaveModule(widgets) {
    CMAKE_QT_MODULES_UNDER_TEST = designer help uitools
}

CONFIG += ctest_testcase
