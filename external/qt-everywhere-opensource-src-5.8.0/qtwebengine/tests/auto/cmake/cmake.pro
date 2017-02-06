
# Cause make to do nothing.
TEMPLATE = subdirs

CMAKE_QT_MODULES_UNDER_TEST = webengine
qtHaveModule(widgets): CMAKE_QT_MODULES_UNDER_TEST += webenginewidgets

CONFIG += ctest_testcase
