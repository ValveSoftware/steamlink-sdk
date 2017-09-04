
# Cause make to do nothing.
TEMPLATE = subdirs

win32 {
enable_when_unbroken_in_CI: CONFIG += ctest_testcase
} else {
CONFIG += ctest_testcase
}
