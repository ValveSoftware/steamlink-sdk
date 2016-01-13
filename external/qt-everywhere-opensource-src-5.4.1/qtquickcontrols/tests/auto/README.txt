
Here are some tips if you need to run the Qt Quick Controls auto tests.

 - Testplugin

Some autotests require the test plugin under testplugin/QtQuickControlsTests.

The test plugin is not installed (i.e. to the qml folder), so
in order for the tst_controls to find it, you can either:

- Run make check in the controls folder. The plugin will be found
at run time because IMPORTPATH is defined in the pro file.

- In Qt Creator run settings or in the console, set QML2_IMPORT_PATH
macro to the testplugin path. At run time QML2_IMPORT_PATH is used by
by qmlscene to find imports required.
i.e: export QML2_IMPORT_PATH=<path_qtquickcontrols_git_clone>/tests/auto/testplugin

- Use the -import command-line option:
$ cd build/qt5/qtquickcontrols/tests/auto/controls
$ ./tst_controls -import ../testplugin

 - Running specific tests:

i) It is possible to run a single file using the -input option. For example:

$ ./tst_controls -input data/test.qml

$ ./tst_controls -input <full_path>/test.qml

Specifying the full path to the qml test file is for example needed for shadow builds.


ii) The -functions command-line option will return a list of the current tests functions.
It is possible to run a single test function using the name of the test function as an argument. For example:

tst_controls Test_Name::function1

