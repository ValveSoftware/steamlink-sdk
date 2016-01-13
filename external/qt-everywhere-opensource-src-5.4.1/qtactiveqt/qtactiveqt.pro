CONFIG += tests_need_tools
load(qt_parts)

!win32|winrt|wince {
    message("ActiveQt is a Windows Desktop-only module. Will just generate a docs target.")
    SUBDIRS = src
}
