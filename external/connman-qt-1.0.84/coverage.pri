COVERAGE_EXCLUDES = \
    /usr/* \
    */moc_*.cpp \
    */*.moc \
    */qrc_*.cpp \
    /build/* \
    /tests/* \

coverage.CONFIG = phony
coverage.commands = '\
        echo "--- coverage: extracting info" \
        && lcov -c -o lcov.info -d . \
        && echo "--- coverage: excluding $$join(COVERAGE_EXCLUDES, ", ")" \
        && lcov -r lcov.info $$join(COVERAGE_EXCLUDES, "' '", "'", "'") -o lcov.info \
        && echo "--- coverage: generating html" \
        && genhtml -o coverage lcov.info --demangle-cpp \
        && echo -e "--- coverage: done\\n\\n\\tfile://$${OUT_PWD}/coverage/index.html\\n" \
        '

QMAKE_EXTRA_TARGETS += coverage
