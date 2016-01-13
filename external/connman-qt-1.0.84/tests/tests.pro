include(tests_common.pri)

TEMPLATE = subdirs
SUBDIRS = \
    ut_agent.pro \
    ut_clock.pro \
    ut_manager.pro \
    ut_service.pro \
    ut_session.pro \
    ut_technology.pro \

runtest_sh.path = $${INSTALL_TESTDIR}
runtest_sh.files = runtest.sh
INSTALLS += runtest_sh

tests_xml.path = $${INSTALL_TESTDIR}
tests_xml.files = tests.xml
tests_xml.CONFIG = no_check_exist
INSTALLS += tests_xml

configure_tests_xml.target = tests.xml
configure_tests_xml.depends = $${PWD}/tests.xml.in
configure_tests_xml.commands = 'sed -e "s!@INSTALL_TESTDIR@!$${INSTALL_TESTDIR}!g" \
    < $${PWD}/tests.xml.in > tests.xml'
QMAKE_EXTRA_TARGETS += configure_tests_xml

make_default.depends += configure_tests_xml
make_default.CONFIG = phony
QMAKE_EXTRA_TARGETS += make_default

check.depends = make_default
check.CONFIG = phony recursive
QMAKE_EXTRA_TARGETS += check
