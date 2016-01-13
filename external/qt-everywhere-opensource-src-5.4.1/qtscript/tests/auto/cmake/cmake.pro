
# Cause make to do nothing.
TEMPLATE = subdirs

CMAKE_QT_MODULES_UNDER_TEST = script
qtHaveModule(widgets): CMAKE_QT_MODULES_UNDER_TEST += scripttools

CONFIG += ctest_testcase
